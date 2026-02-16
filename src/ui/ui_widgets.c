static UISignal ui_button(String8 text) {
        ui_push_width(uiSizeTextContent(0));
        ui_push_height(uiSizeTextContent(0));

        UIElement *element = ui_build_element_from_string(
            UI_ELEMENTFLAG_Clickable | UI_ELEMENTFLAG_DrawText, text);
        UISignal sig = ui_signal_from_element(element);

        sig.element->borderSize = 2.0f;

        ui_element_emboss_from_solid(element);
        if (sig.flags & UI_INTERACTIONFLAG_LeftPressed) {
                ui_element_flip_embossment(element);
        }

        ui_pop_width();
        ui_pop_height();

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
