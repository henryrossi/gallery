class UiAttr:
    def __init__(self, name_lower, name_func, t):
        self.name_lower = name_lower
        self.name_upper = name_lower[:]
        self.name_upper = self.name_upper[0].upper() + self.name_upper[1:]
        self.name_func = name_func
        self.type = t


ui_attrs = [
    UiAttr("parent", "parent", "UIElement *"),
    UiAttr("textColor", "text_color", "Vec4"),
    UiAttr("backgroundColor", "background_color", "Vec4"),
    UiAttr("width", "width", "SemanticSize"),
    UiAttr("height", "heigth", "SemanticSize"),
]

with open("ui.h", "w+") as f:
    f.write("#define UIStackNodesDecl \\\n")
    for attr in ui_attrs:
        node_type = "UI" + attr.name_upper + "Node"
        f.write("typedef struct " + node_type + " " + node_type + ";\\\n")
        f.write(
            "struct "
            + node_type
            + "{"
            + node_type
            + "*next;"
            + attr.type
            + " v;}; \\\n"
        )
    f.write("\n")

    f.write("#define UIStacksDecl \\\n")
    for attr in ui_attrs:
        node_type = "UI" + attr.name_upper + "Node"
        stack_bottom = attr.name_lower + "StackBottom"
        bottomDecl = node_type + " " + stack_bottom + ";\\\n"
        f.write(bottomDecl)

        stackDecl = (
            "struct{"
            + node_type
            + "*top;"
            + node_type
            + "*free;}"
            + attr.name_lower
            + "Stack;\\\n"
        )
        f.write(stackDecl)
    f.write("\n")

with open("ui.c", "w+") as f:
    f.write("#define UIStackFuncImpl()\\\n")
    for attr in ui_attrs:
        f.write(
            "static void ui_push_"
            + attr.name_func
            + "("
            + attr.type
            + " v) { UIStackPushImpl(ui_state, "
            + attr.name_upper
            + ", "
            + attr.name_lower
            + ", v)}\\\n"
        )
        f.write(
            "static "
            + attr.type
            + " ui_top_"
            + attr.name_func
            + "(void) { UIStackTopImpl(ui_state, "
            + attr.name_upper
            + ", "
            + attr.name_lower
            + ")}\\\n"
        )
        f.write(
            "static "
            + attr.type
            + " ui_pop_"
            + attr.name_func
            + "(void) { UIStackPopImpl(ui_state, "
            + attr.name_upper
            + ", "
            + attr.name_lower
            + ")}\\\n"
        )

    f.write("\n")
