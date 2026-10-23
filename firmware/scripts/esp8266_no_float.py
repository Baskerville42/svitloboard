Import("env")


if env.get("PIOPLATFORM") == "espressif8266":
    original = list(env.get("LINKFLAGS", []))
    filtered = []
    index = 0
    while index < len(original):
        if (
            original[index] == "-u"
            and index + 1 < len(original)
            and original[index + 1] in ("_printf_float", "_scanf_float")
        ):
            index += 2
            continue
        filtered.append(original[index])
        index += 1
    env.Replace(LINKFLAGS=filtered)
