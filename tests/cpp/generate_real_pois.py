import json


def main():
    json_path = "backend/engine_input_dump.json"
    out_hpp = "real_pois_data.hpp"

    try:
        with open(json_path, "r") as f:
            lines = f.readlines()
            last_line = lines[-1].strip()
    except Exception as e:
        print(f"Error reading JSON: {e}")
        return

    data = json.loads(last_line)

    pois = data.get("pois", [])
    durations = data.get("durations", [])
    costs = data.get("costs", [])
    config = data.get("config", {})

    with open(out_hpp, "w") as f:
        f.write("#pragma once\n\n")
        f.write('#include "paladio/engine.hpp"\n')
        f.write("#include <vector>\n\n")
        f.write("namespace paladio::core::test_data {\n\n")

        f.write("inline std::vector<POI> get_real_pois() {\n")
        f.write("    return {\n")
        for p in pois:
            t = p["type"]
            c = p["cost"]
            s = p["score"]
            e = p["earliest_time"]
            latest = p["latest_time"]
            d = p["duration"]
            m = "true" if p.get("is_mandatory", False) else "false"
            f.write(
                f"        {{static_cast<NodeType>({t}), {c}, {s}, {e}, {latest}, {d}, {m}}},\n"
            )
        f.write("    };\n")
        f.write("}\n\n")

        f.write("inline std::vector<TransitInfo> get_real_transits() {\n")
        f.write("    return {\n")
        for i in range(len(durations)):
            dur = durations[i]
            cost = costs[i]
            f.write(f"        {{{dur}, {cost}}},\n")
        f.write("    };\n")
        f.write("}\n\n")

        f.write("inline OptimizationConfig get_real_config() {\n")
        f.write("    OptimizationConfig config;\n")

        # map config fields
        f.write(f"    config.max_budget = {config.get('max_budget', 1000.0)};\n")
        f.write(f"    config.max_idle_time = {config.get('max_idle_time', 240)};\n")
        if "breakfast_deadline" in config:
            f.write(
                f"    config.breakfast_deadline = {config['breakfast_deadline']};\n"
            )
        if "lunch_deadline" in config:
            f.write(f"    config.lunch_deadline = {config['lunch_deadline']};\n")
        if "dinner_deadline" in config:
            f.write(f"    config.dinner_deadline = {config['dinner_deadline']};\n")
        if "end_time_limit" in config:
            f.write(f"    config.end_time_limit = {config['end_time_limit']};\n")
        if "start_node_index" in config:
            f.write(f"    config.start_node_index = {config['start_node_index']};\n")
        if "end_node_index" in config:
            f.write(f"    config.end_node_index = {config['end_node_index']};\n")

        f.write("    return config;\n")
        f.write("}\n\n")
        f.write("} // namespace paladio::core::test_data\n")


if __name__ == "__main__":
    main()
