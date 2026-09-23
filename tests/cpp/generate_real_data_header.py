import asyncio
import math
import os
import sys

# Add backend to path so we can import the repository
sys.path.append(os.path.abspath("../../backend"))
from app.adapters.repositories.sql_poi_repository import SqlPoiRepository


def haversine_distance(lat1, lon1, lat2, lon2):
    R = 6371.0  # km
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = (
        math.sin(dlat / 2) ** 2
        + math.cos(math.radians(lat1))
        * math.cos(math.radians(lat2))
        * math.sin(dlon / 2) ** 2
    )
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c


async def generate():
    repo = SqlPoiRepository()
    pois = await repo.find_by_city("Madrid")
    pois = pois[:30]  # Limit to 30 to stay under DFS engine 64 bitmask limit

    all_pois = []

    # We add a dummy hotel as base node, required for start/end routing
    all_pois.append(
        {
            "id": "HOTEL-0",
            "name": "Base Hotel",
            "type": 1,  # NodeType::HOTEL
            "cost": 0.0,
            "score": 10.0,
            "earliest_time": 0,
            "latest_time": 1440,
            "duration": 10,
            "is_mandatory": "true",
            "lat": 40.4168,
            "lon": -3.7038,
        }
    )

    for p in pois:
        cost = p.financials.estimated_cost or 0.0
        score = p.scoring.rating * 10.0 if p.scoring.rating else 30.0
        duration = p.schedule.recommended_duration_minutes or 60

        all_pois.append(
            {
                "id": p.id,
                "name": p.name,
                "type": 0,  # NodeType::ATTRACTION
                "cost": float(cost),
                "score": float(score),
                "earliest_time": 540,
                "latest_time": 1080,
                "duration": int(duration),
                "is_mandatory": "false",
                "lat": float(p.location.latitude),
                "lon": float(p.location.longitude),
            }
        )

    hpp_content = """#pragma once

#include "paladio/engine.hpp"
#include <vector>

namespace paladio::core::test_data {

#include <string>

inline std::vector<std::string> get_real_poi_names() {
    return {
"""
    for p in all_pois:
        name_esc = p["name"].replace('"', '\\"')
        hpp_content += f'        "{name_esc}",\n'

    hpp_content += """    };
}

inline std::vector<POI> get_real_pois() {
    return {
"""
    for p in all_pois:
        t = p["type"]
        c = p["cost"]
        s = p["score"]
        e = p["earliest_time"]
        latest = p["latest_time"]
        d = p["duration"]
        m = p["is_mandatory"]
        hpp_content += f"        {{static_cast<NodeType>({t}), {c:.1f}, {s:.1f}, {e}, {latest}, {d}, {m}}},\n"

    hpp_content += """    };
}

inline std::vector<TransitInfo> get_real_transits() {
    return {
"""
    for i in range(len(all_pois)):
        for j in range(len(all_pois)):
            if i == j:
                hpp_content += "        {0, 0.0},\n"
            else:
                dist = haversine_distance(
                    all_pois[i]["lat"],
                    all_pois[i]["lon"],
                    all_pois[j]["lat"],
                    all_pois[j]["lon"],
                )
                duration = int(max(5.0, dist * 15.0))  # 15 mins per km
                hpp_content += f"        {{{duration}, 0.0}},\n"

    hpp_content += """    };
}

inline OptimizationConfig get_real_config() {
    OptimizationConfig config;
    config.max_budget = 200.0;
    config.max_idle_time = 1440;
    config.breakfast_deadline = -1;
    config.lunch_deadline = -1;
    config.dinner_deadline = -1;
    config.end_time_limit = 1440;
    config.start_node_index = 0;
    config.end_node_index = 0;
    return config;
}

} // namespace paladio::core::test_data
"""
    with open("real_pois_data.hpp", "w") as f:
        f.write(hpp_content)


if __name__ == "__main__":
    asyncio.run(generate())
