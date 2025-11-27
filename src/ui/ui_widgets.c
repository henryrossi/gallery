static UISignal ui_button(String8 text, SemanticSize x, SemanticSize y) {
        UIElement *element = ui_build_element_from_string(text);
        element->size[AXIS2D_X] = x;
        element->size[AXIS2D_Y] = y;
        UISignal res = { .element = element };
        return res;
}
