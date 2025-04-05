def load_env(path="/secrets.env"):
    env = {}
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if "=" in line:
                    k, v = line.split("=", 1)
                    v = v.strip()
                    if v.startswith((""", "'")) and v.endswith((""", "'")):
                        v = v[1:-1]
                    env[k.strip()] = v
    except Exception as e:
        print("Failed to load env file:", str(e))
    return env
