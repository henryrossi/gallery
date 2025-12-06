static UISignal ui_button(String8 text) {
        UIElement *element = ui_build_element_from_string(
            UI_ELEMENTFLAG_Clickable | UI_ELEMENTFLAG_DrawText, text);
        UISignal sig = ui_signal_from_element(element);
        return sig;
}

static UISignal ui_buttonf(char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        UIElement *element = ui_build_element_from_stringfv(
            UI_ELEMENTFLAG_Clickable | UI_ELEMENTFLAG_DrawText, fmt, args);
        UISignal sig = ui_signal_from_element(element);
        va_end(args);
        return sig;
}
