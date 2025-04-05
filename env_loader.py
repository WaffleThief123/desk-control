def load_env(path=".env") -> dict[str,str]:
    env: dict[str,str] = {}
    try:
        with open(path) as f:
            for line in f:
                if "=" in line:
                    k, v = line.strip().split("=", 1)
                    env[k] = v
    except Exception as e:
        print("Failed to load .env file:", e)
    return env
