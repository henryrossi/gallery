#include "buffer.h"
#include "coordTransform.h"
#include "glyph.h"
#include "graphicsPipeline.h"
#include "matrix.h"
#include "quad.h"

#include <stdio.h>
#include <string.h>

const static char *csFilename = "color.save";

const static float panelScale = 0.25;
const static float currentScale = panelScale;
const static float historyScale = panelScale / 8;
const static float previewScale = panelScale / 4;
const static float sliderScale = panelScale * 0.9;

static void setCurrentColor(ControlPanel *cp, Vec3 *color) {
        for (int i = CONTROL_PANEL_HISTORY_16; i > 0; i--) {
                cp->quadUniforms[i].color = cp->quadUniforms[i - 1].color;
        }
        cp->quadUniforms[0].color = *color;
}

static void pickCurrentColorFromHistory(ControlPanel *panel) {
        uint32_t picked = panel->clicked;
        Vec3 tmp = panel->quadUniforms[picked].color;
        for (int i = picked; i > 0; i--) {
                panel->quadUniforms[i].color = panel->quadUniforms[i - 1].color;
        }
        panel->quadUniforms[0].color = tmp;
}

static void updateColorCreatorPreview(ControlPanel *panel, float mouseX,
                                      float mouseY) {
        uint32_t clicked = panel->clicked;
        BoundingBox bar
            = getQuadBoundingBox(&panel->quadUniforms[clicked - 1].mvp);

        float newButtonPos = mouseX;
        if (newButtonPos < bar.pos.x) {
                newButtonPos = bar.pos.x;
        } else if (newButtonPos > bar.pos.x + bar.extent.x) {
                newButtonPos = bar.pos.x + bar.extent.x;
        }

        float newColorValue = (newButtonPos - bar.pos.x) / bar.extent.x;

        if (clicked == CONTROL_PANEL_CREATOR_R_BUTTON) {
                panel->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color.x
                    = newColorValue;
        } else if (clicked == CONTROL_PANEL_CREATOR_G_BUTTON) {
                panel->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color.y
                    = newColorValue;
        } else if (clicked == CONTROL_PANEL_CREATOR_B_BUTTON) {
                panel->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color.z
                    = newColorValue;
        }
}

static void formatControlPanelColors(ControlPanel *cp) {
        FILE *fp = fopen(csFilename, "rb");
        if (fp) {
                Vec3 colors[COLOR_HISTORY_LENGTH + 1] = { 0 };
                fread(colors, sizeof(colors), 1, fp);
        } else {
                Vec3 colors[COLOR_HISTORY_LENGTH + 1] = {
                        { 1.0, 0.5, 0.0 }, { 0.0, 0.0, 0.0 }, { 1.0, 1.0, 1.0 },
                        { 0.5, 0.5, 0.5 }, { 0.5, 1.0, 0.0 }, { 0.5, 0.0, 1.0 },
                        { 0.5, 0.5, 1.0 }, { 0.5, 1.0, 0.5 }, { 0.0, 1.0, 0.5 },
                        { 0.0, 0.5, 1.0 }, { 1.0, 0.2, 0.0 }, { 0.5, 0.0, 1.0 },
                        { 0.0, 1.0, 1.0 }, { 1.0, 0.0, 1.0 }, { 1.0, 1.0, 0.0 },
                        { 0.0, 0.0, 0.5 }, { 0.5, 0.2, 0.8 },
                };
                for (int i = 0; i < COLOR_HISTORY_LENGTH; i++) {
                        cp->quadUniforms[i].color = colors[i];
                }
        }

        Vec3 white = { 1.0, 1.0, 1.0 };
        cp->quadUniforms[CONTROL_PANEL_CREATOR_R_BAR].color = white;
        cp->quadUniforms[CONTROL_PANEL_CREATOR_G_BAR].color = white;
        cp->quadUniforms[CONTROL_PANEL_CREATOR_B_BAR].color = white;
}
