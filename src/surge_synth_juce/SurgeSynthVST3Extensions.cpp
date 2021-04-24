/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include <JuceHeader.h>
#include <iostream>

#include "SurgeSynthProcessor.h"
#include "SurgeSynthEditor.h"
#include "SurgeSynthFlavorExtensions.h"

void SurgeSynthProcessorSpecificExtensions(SurgeSynthProcessor *p, SurgeSynthesizer *s)
{
    std::cout << "SynthProcessor: " << __FILE__ << std::endl;
}
void SurgeSynthEditorSpecificExtensions(SurgeSynthEditor *e, SurgeGUIEditor *sed)
{
    std::cout << "SynthEditor: " << __FILE__ << std::endl;
}

#if YOU_WANT_TO_SEE_THE_OLD_MENU_CODE
/*
 * When I removed TARGET_VST3 from everywhere I kept the menu code herefor when we restore it
 */
// SGE.h
Steinberg::Vst::IContextMenu *
addVst3MenuForParams(VSTGUI::COptionMenu *c, const SurgeSynthesizer::ID &,
                     int &eid); // just a noop if you aren't a vst3 of course

#include "pluginterfaces/vst/ivstcontextmenu.h"
#include "pluginterfaces/base/ustring.h"

#include "vstgui/lib/cvstguitimer.h"

#include "SurgeVst3Processor.h"

template <typename T> struct RememberForgetGuard
{
    RememberForgetGuard(T *tg)
    {
        t = tg;

        int rc = -1;
        if (t)
            rc = t->addRef();
    }
    RememberForgetGuard(const RememberForgetGuard &other)
    {
        int rc = -1;
        if (t)
        {
            rc = t->release();
        }
        t = other.t;
        if (t)
        {
            rc = t->addRef();
        }
    }
    ~RememberForgetGuard()
    {
        if (t)
        {
            t->release();
        }
    }
    T *t = nullptr;
};

// later on

Steinberg::Vst::IContextMenu *hostMenu = nullptr;
SurgeSynthesizer::ID mid;
if (synth->fromSynthSideId(modsource - ms_ctrl1 + metaparam_offset, mid))
    hostMenu = addVst3MenuForParams(contextMenu, mid, eid);
// then ...

if (hostMenu)
    hostMenu->release();

#if TARGET_VST3
auto hostMenu = addVst3MenuForParams(contextMenu, synth->idForParameter(p), eid);
#endif

#if TARGET_VST3
if (hostMenu)
    hostMenu->release();
#endif

Steinberg::Vst::IContextMenu *SurgeGUIEditor::addVst3MenuForParams(VSTGUI::COptionMenu *contextMenu,
                                                                   const SurgeSynthesizer::ID &pid,
                                                                   int &eid)
{
    CRect menuRect;
    Steinberg::Vst::IComponentHandler *componentHandler = getController()->getComponentHandler();
    Steinberg::FUnknownPtr<Steinberg::Vst::IComponentHandler3> componentHandler3(componentHandler);
    Steinberg::Vst::IContextMenu *hostMenu = nullptr;
    if (componentHandler3)
    {
        std::stack<COptionMenu *> menuStack;
        menuStack.push(contextMenu);
        std::stack<int> eidStack;
        eidStack.push(eid);

        Steinberg::Vst::ParamID param = pid.getDawSideId();
        hostMenu = componentHandler3->createContextMenu(this, &param);

        int N = hostMenu ? hostMenu->getItemCount() : 0;
        if (N > 0)
        {
            contextMenu->addSeparator();
            eid++;
        }

        std::deque<COptionMenu *> parentMenus;
        for (int i = 0; i < N; i++)
        {
            Steinberg::Vst::IContextMenu::Item item = {0};
            Steinberg::Vst::IContextMenuTarget *target = {0};

            hostMenu->getItem(i, item, &target);

            // char nm[1024];
            // Steinberg::UString128(item.name, 128).toAscii(nm, 1024);
#if WINDOWS
            // https://stackoverflow.com/questions/32055357/visual-studio-c-2015-stdcodecvt-with-char16-t-or-char32-t
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> conversion;
            std::string nm = conversion.to_bytes((wchar_t *)(item.name));
#else
            std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conversion;
            std::string nm = conversion.to_bytes((char16_t *)(item.name));
#endif

            if (nm[0] ==
                '-') // FL sends us this as a separator with no VST indication so just strip the '-'
            {
                int pos = 1;
                while (nm[pos] == ' ' && nm[pos] != 0)
                    pos++;
                nm = nm.substr(pos);
            }

            auto itag = item.tag;
            /*
            ** Leave this here so we can debug if another vst3 problem comes up
            std::cout << nm << " FL=" << item.flags << " jGS=" <<
            Steinberg::Vst::IContextMenuItem::kIsGroupStart
            << " and=" << ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart )
            << " IGS="
            << ( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart ) ==
            Steinberg::Vst::IContextMenuItem::kIsGroupStart ) << " IGE="
            << ( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupEnd ) ==
            Steinberg::Vst::IContextMenuItem::kIsGroupEnd ) << " "
            << std::endl;

            if( item.flags != 0 )
               printf( "FLAG %d IGS %d IGE %d SEP %d\n",
                      item.flags,
                      item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart,
                      item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupEnd,
                      item.flags & Steinberg::Vst::IContextMenuItem::kIsSeparator
                      );
                      */
            if ((item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart) ==
                Steinberg::Vst::IContextMenuItem::kIsGroupStart)
            {
                COptionMenu *subMenu = new COptionMenu(
                    menuRect, 0, 0, 0, 0,
                    VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
                menuStack.top()->addEntry(subMenu, nm.c_str());
                menuStack.push(subMenu);
                subMenu->forget();
                eidStack.push(0);

                /*
                  VSTGUI doesn't seem to allow a disabled or checked grouping menu.
                  if( item.flags & Steinberg::Vst::IContextMenuItem::kIsDisabled )
                  {
                  subMenu->setEnabled(false);
                  }
                  if( item.flags & Steinberg::Vst::IContextMenuItem::kIsChecked )
                  {
                  subMenu->setChecked(true);
                  }
                */
            }
            else if ((item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupEnd) ==
                     Steinberg::Vst::IContextMenuItem::kIsGroupEnd)
            {
                menuStack.pop();
                eidStack.pop();
            }
            else if (item.flags & Steinberg::Vst::IContextMenuItem::kIsSeparator)
            // separator not group end. Thanks for the insane definition of these constants VST3!
            // (See #3090)
            {
                menuStack.top()->addSeparator();
            }
            else
            {
                RememberForgetGuard<Steinberg::Vst::IContextMenuTarget> tg(target);
                RememberForgetGuard<Steinberg::Vst::IContextMenu> hm(hostMenu);

                auto menu = addCallbackMenu(
                    menuStack.top(), nm, [this, hm, tg, itag]() { tg.t->executeMenuItem(itag); });
                eidStack.top()++;
                if (item.flags & Steinberg::Vst::IContextMenuItem::kIsDisabled)
                {
                    menu->setEnabled(false);
                }
                if (item.flags & Steinberg::Vst::IContextMenuItem::kIsChecked)
                {
                    menu->setChecked(true);
                }
            }
            // hostMenu->addItem(item, &target);
        }
        eid = eidStack.top();
    }
    return hostMenu;
}
#endif
