static UISignal ui_button(String8 text, SemanticSize x, SemanticSize y) {
        UIElement *element = ui_build_element_from_string(
            UI_ELEMENTFLAG_Clickable | UI_ELEMENTFLAG_DrawText, text);
        element->size[UI_AXIS2D_X] = x;
        element->size[UI_AXIS2D_Y] = y;
        UISignal sig = ui_signal_from_element(element);
        return sig;
}
