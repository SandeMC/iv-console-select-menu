#include "plugin.h"

#include "audFrontendAudioEntity.h"
#include "T_CB_Generic.h"

#include "CControl.h"
#include "CControlMgr.h"
#include "CFont.h"
#include "CHudColours.h"
#include "CMenuManager.h"
#include "CPad.h"
#include "CTxdStore.h"
#include "CText.h"

#include "rage/ioValue.h"

#include "Utility.h"

#include <cstdio>
#include <cstring>

using namespace plugin;

class ConsoleSelectMenuIV {
public:
    static inline CSprite2d gtaiv = {};
    static inline CSprite2d gtatlad = {};
    static inline CSprite2d gtatbogt = {};
    static inline CSprite2d gta = {};
    static inline int32_t selectedEpisode = 1;
    static inline int32_t prevEpisode = 1;
    static inline bool drawMouse = false;
    static inline bool checkHover = false;

    static inline bool bSkipTitleMenu = false;
    static inline bool bUseCELogo = true;
    static inline bool bEnabled = false;
    static inline bool bShuttingDown = false;
    static inline bool bTitleMenuPatched = false;
    static inline uintptr_t titleMenuAddress = 0;
    static inline uint8_t titleMenuBytes[5] = {};

    static void DrawCharacter(CSprite2d* sprite, int32_t i, bool selected) {
        float hw = 205.0f;
        float h = 550.0f;
        float spacing = (hw * 0.5f) + 56.0f;
        float y = 86.0f;

        if (selected) {
            float size = 24.0f;
            hw += size / 2;
            h += size;
            y -= hw * 0.06f;
        }

        float x = (DEFAULT_SCREEN_WIDTH / 2) - hw - spacing - 6.0f;

        x += ((hw + spacing) * i);

        rage::fwRect rect;
        rect.left = ScaleXKeepCentered(x - hw);
        rect.top = ScaleY(y);
        rect.right = ScaleXKeepCentered(x + hw);
        rect.bottom = rect.top + ScaleY(h);

        sprite->SetRenderState();
        sprite->Draw(rect, rage::Color32(255, 255, 255, selected ? 255 : 90));

        rect.left += ScaleX(64.0f);
        rect.right -= ScaleX(64.0f);
        rect.bottom -= ScaleY(64.0f);
        if (CheckHover(rect) && drawMouse) {
            prevEpisode = selectedEpisode;
            selectedEpisode = i;
            checkHover = true;
        }
    }

    static const rage::ioValue* GetAction(int32_t action) {
        return &CControlMgr::m_controls[0].m_inputs[action];
    }

    static const bool GetAxisLeft() {
        CPad* pad = CPad::GetPad(0);
        return pad && pad->NewState.LeftStickX < 0 && pad->OldState.LeftStickX >= 0;
    }

    static const bool GetAxisRight() {
        CPad* pad = CPad::GetPad(0);
        return pad && pad->NewState.LeftStickX > 0 && pad->OldState.LeftStickX <= 0;
    }

    static bool GetLeft() {
        return (!GetAction(rage::INPUT_FRONTEND_LEFT)->m_nNewState && GetAction(rage::INPUT_FRONTEND_LEFT)->m_nOldState) ||
            (!GetAction(rage::INPUT_FRONTEND_AXIS_LEFT)->m_nNewState && GetAction(rage::INPUT_FRONTEND_AXIS_LEFT)->m_nOldState) ||
            (GetAxisLeft());
    }

    static bool GetRight() {
        return (!GetAction(rage::INPUT_FRONTEND_RIGHT)->m_nNewState && GetAction(rage::INPUT_FRONTEND_RIGHT)->m_nOldState) ||
            (!GetAction(rage::INPUT_FRONTEND_AXIS_RIGHT)->m_nNewState && GetAction(rage::INPUT_FRONTEND_AXIS_RIGHT)->m_nOldState) ||
            (GetAxisRight());
    }

    static bool GetEnter() {
        return !GetAction(rage::INPUT_FRONTEND_ACCEPT)->m_nNewState && GetAction(rage::INPUT_FRONTEND_ACCEPT)->m_nOldState;
    }
    
    static bool GetQuit() {
        return !GetAction(rage::INPUT_FRONTEND_PAUSE)->m_nNewState && GetAction(rage::INPUT_FRONTEND_PAUSE)->m_nOldState;
    }

    static bool GetLMB() {
        if (!checkHover)
            return false;

        return CControlMgr::m_controls[0].m_inputs[rage::INPUT_MOUSE_LMB].m_nNewState && !CControlMgr::m_controls[0].m_inputs[rage::INPUT_MOUSE_LMB].m_nOldState;
    }

    static void Dummy() {

    }

    static void SetEpisodeString() {
        int32_t ep = EPISODE_IV;
        switch (selectedEpisode) {
            case 0:
                ep = EPISODE_TBOGT;
                break;
            case 2:
                ep = EPISODE_TLAD;
                break;
        }

        static char buf[2];
        snprintf(buf, sizeof(buf), "%d", ep);
        CMenuManager::m_EpisodeStr = buf;
    }

    static void DrawHelpText() {
        CMenuManager::SetHelpText("MO_EXITGAM", "PAD_START", 1);
        CMenuManager::SetHelpText("E1_SELECT", "FE_ACCEPT", 0);
        CMenuManager::SetHelpText("FE_HLP4", "FE_BUTL", 0);
        CMenuManager::DrawHelpText();
    }

    static void ReadSettings() {
        config_file config(true, false);
        bSkipTitleMenu = config["bSkipTitleMenu"].asBool(false);
        bUseCELogo = config["bUseCELogo"].asBool(true);
    }

    static bool LoadTextures() {
        const int32_t slot = CTxdStore::AddTxdSlot("console_select_menu");
        if (slot < 0) {
            return false;
        }

        if (!CTxdStore::LoadTxd(slot, "platform:/textures/console_select_menu")) {
            CTxdStore::RemoveTxdSlot(slot);
            return false;
        }

        CTxdStore::AddRef(slot);
        CTxdStore::PushCurrentTxd();
        CTxdStore::SetCurrentTxd(slot);

        gtaiv.SetTexture("iv");
        gtatlad.SetTexture("tlad");
        gtatbogt.SetTexture("tbogt");
        gta.SetTexture("gta");

        CTxdStore::PopCurrentTxd();

        const bool ready = gtaiv.m_pTexture && gtatlad.m_pTexture && gtatbogt.m_pTexture;
        if (!ready) {
            CTxdStore::RemoveTxdSlot(slot);
        }

        return ready;
    }

    static uintptr_t TitleMenuAddress() {
        return plugin::pattern::Get("68 8A 00 8B F8 EB 02 33 FF 8B CF E8 ? ? ? ?", -0x30);
    }

    static uintptr_t LoadingScreenCallAddress() {
        return plugin::pattern::Get("E8 ? ? ? ? F3 0F 10 4C 24 ? F3 0F 10 5C 24 ? 83 C4 08");
    }

    static void PatchTitleMenu() {
        titleMenuAddress = TitleMenuAddress();
        if (!titleMenuAddress) {
            return;
        }

        if (plugin::patch::GetUInt(titleMenuAddress) != 0x002CA164
            || plugin::patch::GetUShort(titleMenuAddress + 6) != 0xEC83) {
            return;
        }

        plugin::patch::GetRaw(titleMenuAddress, titleMenuBytes, sizeof(titleMenuBytes));
        plugin::patch::RedirectJump(titleMenuAddress, Dummy);
        bTitleMenuPatched = true;
    }

    static void RestoreTitleMenu() {
        if (!bTitleMenuPatched) {
            return;
        }

        plugin::patch::SetRaw(titleMenuAddress, titleMenuBytes, sizeof(titleMenuBytes));
        bTitleMenuPatched = false;
    }

    static void Initialise() {
        ReadSettings();

        if (!bTitleMenuPatched || !LoadTextures()) {
            RestoreTitleMenu();
            return;
        }

        bEnabled = true;
    }

    static void Shutdown() {
        bShuttingDown = true;
        bEnabled = false;
    }

    ConsoleSelectMenuIV() {
        PatchTitleMenu();

        plugin::Events::initEngineEvent += []() {
            Initialise();
        };

        plugin::Events::shutdownEngineEvent += []() {
            Shutdown();
        };

        plugin::Events::menuProcessEvent += []() {
            if (!bEnabled || bShuttingDown)
                return;

            if (CMenuManager::m_DefaultFrontend || CMenuManager::m_Refresh)
                return;

            if (CMenuManager::m_CurrState > 0)
                return;

            if (CMenuManager::m_CurrScreen != MENUPAGE_MAIN_SELECT)
                return;

            if (GetLeft()) {
                prevEpisode = selectedEpisode;
                selectedEpisode--;
                drawMouse = false;
            }

            if (GetRight()) {
                prevEpisode = selectedEpisode;
                selectedEpisode++;
                drawMouse = false;
            }

            if (selectedEpisode < 0)
                selectedEpisode = 2;

            if (selectedEpisode > 2)
                selectedEpisode = 0;

            if (selectedEpisode != prevEpisode) {
                prevEpisode = selectedEpisode;
                g_FrontendAudioEntity.PlaySound("FRONTEND_MENU_HIGHLIGHT");
            }

            if (GetLMB() || GetEnter()) {
                // Only avoid playing the sound if the original title menu is not skipped and if enter is pressed
                // (Because normally under this condition the sound plays already, so it would play twice otherwise)
                if (!(!bSkipTitleMenu && GetEnter()))
                    g_FrontendAudioEntity.PlaySound("FRONTEND_MENU_SELECT");

                SetEpisodeString();

                if (bSkipTitleMenu)
                    CMenuManager::m_CurrState = MENUSTATE_START_GAME;
                else {
                    CMenuManager::m_SelectEpisode = 0;
                    CMenuManager::m_DefaultFrontend = 1;
                    CMenuManager::m_Refresh = 1;
                }

                drawMouse = false;
            }

            if (GetQuit()) {
                CMenuManager::m_CurrState = MENUSTATE_QUIT_ASK;
                g_FrontendAudioEntity.PlaySound("FRONTEND_MENU_BACK");
                drawMouse = false;
            }

            if (CControl::m_UsingMouse) {
                drawMouse = true;
            }

            checkHover = false;
        };

        plugin::Events::drawMenuEvent.before += []() {
            if (!bEnabled || bShuttingDown)
                return;

            if (CMenuManager::m_DefaultFrontend || CMenuManager::m_Refresh)
                return;

            if (CMenuManager::m_CurrState > 0)
                return;

            if (CMenuManager::m_CurrScreen != MENUPAGE_MAIN_SELECT)
                return;

            auto dc = new T_CB_Generic_NoArgs([]() {
                CSprite2d::DrawRect({ 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT }, { 0, 0, 0, 255 });

                DrawCharacter(&gtatbogt, 0, selectedEpisode == 0);
                DrawCharacter(&gtatlad, 2, selectedEpisode == 2);
                DrawCharacter(&gtaiv, 1, selectedEpisode == 1);

                DrawHelpText();

                if (drawMouse)
                    CMenuManager::DrawMouseCursor();
            });
            CBaseDC::InitStatic(dc);
        };

        // Use CE logo on intro :P
        static CdeclEvent <AddressList<0, H_CALL>, PRIORITY_AFTER, ArgPick2N<rage::grcTexturePC*, 0, uint8_t, 1>, void(rage::grcTexturePC*, uint8_t)> onSetLoadingScreenTex;
        if (const uintptr_t address = LoadingScreenCallAddress()) {
            onSetLoadingScreenTex.SetRefAddr(address);
            onSetLoadingScreenTex += [](rage::grcTexturePC* tex, uint8_t) {
                if (!bEnabled || !bUseCELogo || !tex || !tex->m_Name || !gta.m_pTexture)
                    return;

                if (!strcmp(tex->m_Name, "pack:/gta.dds"))
                    gta.SetRenderState();
            };
        }
    }
} consoleSelectMenuIV;
