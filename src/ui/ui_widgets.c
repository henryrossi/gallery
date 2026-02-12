static UISignal ui_button(String8 text) {
        f32 lightGrey = 245.0f / 255.0f;
        f32 grey = 190.0f / 255.0f;
        f32 darkGrey = 120.0f / 255.0f;
        Vec4f32 lgv = v4f32(lightGrey, lightGrey, lightGrey, 1);
        Vec4f32 gv = v4f32(grey, grey, grey, 1);
        Vec4f32 dgv = v4f32(darkGrey, darkGrey, darkGrey, 1);
        Vec4f32 bg[4] = { lgv, gv, lgv, gv };

        ui_push_width(uiSizeTextContent(0));
        ui_push_height(uiSizeTextContent(0));

        UIElement *element = ui_build_element_from_string(
            UI_ELEMENTFLAG_Clickable | UI_ELEMENTFLAG_DrawText, text);
        UISignal sig = ui_signal_from_element(element);

        ui_element_bg_colors(sig.element, bg);
        sig.element->borderSize = 2.0f;
        sig.element->borderColor = dgv;

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
