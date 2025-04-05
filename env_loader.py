def load_env(path="/.env"):
    env = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if "=" in line:
                    split_line = line.split("=", 1)
                    k = split_line[0]
                    v = split_line[1]
                    v = v.strip()
                    
                    if (v.startswith('"') and v.endswith('"')) or (v.startswith("'") and v.endswith("'")):
                        v = v[1:-1]
                    env[k.strip()] = v
    except Exception as e:
        print("Failed to load env file: {}".format(repr(e)))
    return env
