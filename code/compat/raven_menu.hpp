/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_RAVEN_MENU_HPP
#define FNTECH3_COMPAT_RAVEN_MENU_HPP

/* Raven's single-player menu files, as the Jedi single-player engines read
 * them. Neither single-player game ships a menu module: jasp.exe 1.01 and
 * jk2sp.exe 1.02a read `ui/menus.txt` and the `.menu` files it names
 * themselves, so fnTech3's engine does too (cl_jedi_sp_ui.cpp). This is the
 * reader: the tokens, the keywords and named values each engine accepts, and
 * the menus, items and scripts the files describe. It needs no engine.
 *
 * Retail, each a keyword table of {name, handler} in this order:
 *
 *              menu keywords       script commands     item keywords
 *   jasp.exe   0x526090 (37, 12B)  0x526258 (33, 8B)   0x526368 (85, 12B)
 *   jk2sp.exe  0x4eac30 (34, 12B)  0x4eadd8 (23, 8B)   0x4eae98 (69, 12B)
 *
 * Named values are null-ended name lists whose index is the value: window
 * styles (jasp.exe 0x526020, jk2sp.exe 0x4eabc4), item types (0x526040,
 * 0x4eabe0; Academy adds ITEM_TYPE_TEXTSCROLL) and alignments (0x526080,
 * 0x4eac1c); item flags are {name, value} pairs, only WINDOW_INACTIVE = 8
 * (0x526010, 0x4eabb4). An unknown name warns and leaves the value as it was.
 *
 * Tokens follow the engines' COM_ParseExt (jasp.exe 0x422cd0, whitespace
 * 0x422c80): characters are signed, so any byte above 0x7f counts as space;
 * a word runs to the next byte at or below space; a quoted string runs to the
 * next quote or the end; `//` and block comments are skipped; at most 1024
 * characters are kept, and a word that reaches 1024 is discarded with a
 * warning. Numbers and colours are read from the keyword's own line
 * (COM_ParseInt/Float); strings may follow on a later line (PC_ParseString
 * reads on past empty line ends).
 *
 * A script is kept as the engines keep it (PC_Script_Parse): every token of
 * the braces in order, a token of two or more characters quoted, each
 * followed by one space; scripts are read back with the same tokenizer when
 * they run. */

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace fntech3::raven::menu {

enum class Dialect { Academy, Outcast };

/* ------------------------------------------------------------------------
 * Tokens
 * ---------------------------------------------------------------------- */

/* The tokens of one text, which must outlive the reader: it keeps a view. */
class Tokens {
public:
    Tokens() = default;
    explicit Tokens(std::string_view text) : text_(text) {}

    /* COM_ParseExt. With allowLineBreaks false, a line end before the next
     * token ends the read with an empty token; the next read starts after it.
     * Once the text has ended every read is empty. */
    std::string Next(bool allowLineBreaks) {
        std::string token;
        if (ended_) return token;
        bool newLines = false;
        for (;;) {
            if (!SkipSpace(newLines)) { ended_ = true; return token; }
            if (newLines && !allowLineBreaks) return token;
            const char c = text_[at_];
            if (c == '/' && Peek(1) == '/') {
                at_ += 2;
                while (at_ < text_.size() && text_[at_] && text_[at_] != '\n') ++at_;
            } else if (c == '/' && Peek(1) == '*') {
                at_ += 2;
                while (at_ < text_.size() && text_[at_] && !(text_[at_] == '*' && Peek(1) == '/')) {
                    if (text_[at_] == '\n') ++line_;
                    ++at_;
                }
                if (at_ < text_.size() && text_[at_]) at_ += 2;
            } else {
                break;
            }
        }
        if (text_[at_] == '"') {
            ++at_;
            while (at_ < text_.size() && text_[at_] && text_[at_] != '"') {
                if (token.size() < kMaxToken) token.push_back(text_[at_]);
                ++at_;
            }
            if (at_ < text_.size() && text_[at_] == '"') ++at_;
            return token;
        }
        do {
            if (token.size() < kMaxToken) token.push_back(text_[at_]);
            ++at_;
        } while (at_ < text_.size() && Above(text_[at_]));
        if (token.size() == kMaxToken) {
            discarded_ = true;
            token.clear();
        }
        return token;
    }

    // PC_ParseString: the next token that is not empty, across line ends.
    bool String(std::string &out) {
        for (;;) {
            out = Next(false);
            if (!out.empty()) return true;
            if (ended_) return false;
        }
    }
    // COM_ParseInt / COM_ParseFloat: one token from this line.
    bool Int(int &out) {
        const std::string token = Next(false);
        if (token.empty()) return false;
        out = std::atoi(token.c_str());
        return true;
    }
    bool Float(float &out) {
        const std::string token = Next(false);
        if (token.empty()) return false;
        out = static_cast<float>(std::atof(token.c_str()));
        return true;
    }

    // What has not been read yet: the rest of a deferred or delayed script.
    std::string Rest() const { return at_ < text_.size() ? std::string(text_.substr(at_)) : std::string(); }

    bool Ended() const { return ended_; }
    int Line() const { return line_; }
    bool Discarded() { const bool value = discarded_; discarded_ = false; return value; }

    static constexpr std::size_t kMaxToken = 1024;

private:
    static bool Above(char c) { return static_cast<signed char>(c) > ' '; }
    char Peek(std::size_t ahead) const { return at_ + ahead < text_.size() ? text_[at_ + ahead] : '\0'; }
    bool SkipSpace(bool &newLines) {
        while (at_ < text_.size() && !Above(text_[at_])) {
            if (!text_[at_]) return false;
            if (text_[at_] == '\n') { ++line_; newLines = true; }
            ++at_;
        }
        return at_ < text_.size();
    }

    std::string_view text_;
    std::size_t at_ = 0;
    int line_ = 1;
    bool ended_ = false;
    bool discarded_ = false;
};

/* ------------------------------------------------------------------------
 * Named values and flags
 * ---------------------------------------------------------------------- */

enum Style : int { StyleEmpty, StyleFilled, StyleGradient, StyleShader, StyleTeamColor, StyleCinematic };
enum Type : int {
    TypeText, TypeButton, TypeRadioButton, TypeCheckBox, TypeEditField, TypeCombo, TypeListBox, TypeModel,
    TypeOwnerDraw, TypeNumericField, TypeSlider, TypeYesNo, TypeMulti, TypeBind, TypeTextScroll
};
enum Align : int { AlignLeft, AlignCenter, AlignRight };

inline constexpr const char *kStyles[] = {"WINDOW_STYLE_EMPTY", "WINDOW_STYLE_FILLED", "WINDOW_STYLE_GRADIENT",
    "WINDOW_STYLE_SHADER", "WINDOW_STYLE_TEAMCOLOR", "WINDOW_STYLE_CINEMATIC"};
inline constexpr const char *kTypes[] = {"ITEM_TYPE_TEXT", "ITEM_TYPE_BUTTON", "ITEM_TYPE_RADIOBUTTON",
    "ITEM_TYPE_CHECKBOX", "ITEM_TYPE_EDITFIELD", "ITEM_TYPE_COMBO", "ITEM_TYPE_LISTBOX", "ITEM_TYPE_MODEL",
    "ITEM_TYPE_OWNERDRAW", "ITEM_TYPE_NUMERICFIELD", "ITEM_TYPE_SLIDER", "ITEM_TYPE_YESNO", "ITEM_TYPE_MULTI",
    "ITEM_TYPE_BIND", "ITEM_TYPE_TEXTSCROLL"};
inline constexpr const char *kAligns[] = {"ITEM_ALIGN_LEFT", "ITEM_ALIGN_CENTER", "ITEM_ALIGN_RIGHT"};

// How many of kTypes each engine knows (Outcast has no text scroll).
inline int TypeCount(Dialect dialect) { return dialect == Dialect::Academy ? 15 : 14; }

/* Window flags. They are the engine's own state, never a file or module
 * contract, but WINDOW_INACTIVE's value is the one the files may name. */
namespace flag {
inline constexpr std::uint32_t MouseOver = 0x1, HasFocus = 0x2, Visible = 0x4, Inactive = 0x8,
    Decoration = 0x10, FadingOut = 0x20, FadingIn = 0x40, MouseOverText = 0x80, InTransition = 0x100,
    ForeColorSet = 0x200, Horizontal = 0x400, ListLeftArrow = 0x800, ListRightArrow = 0x1000,
    ListThumb = 0x2000, ListPageUp = 0x4000, ListPageDown = 0x8000, Orbiting = 0x10000,
    OutOfBoundsClick = 0x20000, Wrapped = 0x40000, AutoWrapped = 0x80000, Forced = 0x100000,
    Popup = 0x200000, BackColorSet = 0x400000, PlayerColor = 0x1000000, ScriptWaiting = 0x2000000,
    InTransitionModel = 0x4000000, IgnoreEscape = 0x8000000;
}

// An item's cvar rule (enableCvar and the rest), and Academy's substring test.
namespace cvarRule {
inline constexpr int Enable = 1, Disable = 2, Show = 4, Hide = 8, Substring = 16;
}

/* ------------------------------------------------------------------------
 * The model
 * ---------------------------------------------------------------------- */

struct Rect { float x = 0, y = 0, w = 0, h = 0; };
using Color = std::array<float, 4>;
using Vec3 = std::array<float, 3>;

struct Window {
    Rect rect;          // where it is drawn: an item's own rect placed in its menu
    Rect rectClient;    // an item's rect as written, relative to its menu
    std::string name, group, background, cinematicName;
    int style = StyleEmpty, border = 0, ownerDraw = 0, ownerDrawFlags = 0;
    float borderSize = 1;
    std::uint32_t flags = 0;
    Color foreColor{1, 1, 1, 1}, backColor{}, borderColor{}, outlineColor{};
    // Runtime: effects (transition, orbit), fades, deferred script.
    Rect rectEffects, rectEffects2;
    int offsetTime = 0, nextTime = 0, delayTime = 0;
    std::string delayedScript;
    int cinematic = -1;
};

struct ColumnInfo { int pos = 0, width = 0, maxChars = 0; };

struct ListBox {
    int startPos = 0, endPos = 0, drawPadding = 0, cursorPos = 0;
    float elementWidth = 0, elementHeight = 0;
    int elementStyle = 0, numColumns = 0;
    std::array<ColumnInfo, 16> columns{};
    std::string doubleClick;
    bool notSelectable = false, scrollHidden = false;
};

struct EditField {
    float minVal = 0, maxVal = 0, defVal = 0, range = 0;
    int maxChars = 0, maxPaintChars = 0, paintOffset = 0;
};

struct Multi {
    std::vector<std::string> cvarList;     // what each choice shows
    std::vector<std::string> cvarStr;      // a string list's values
    std::vector<float> cvarValue;          // a float list's values
    bool strDef = false;
    bool listFeeder = false;               // `cvarStrList feeder`: filled from the item's feeder
};

struct Model {
    int angle = 0, rotationSpeed = 0;
    Vec3 origin{};
    float fovX = 0, fovY = 0;
    Vec3 g2mins{}, g2maxs{};
    std::string g2skin, g2anim;
};

struct TextScroll {
    int startPos = 0, endPos = 0, lineHeight = 0, maxLineChars = 0, drawPadding = 0;
    std::vector<std::string> lines;
};

struct ColorRange { Color color{}; float low = 0, high = 0; };

// Academy's character and saber items (isCharacter, isSaber, isSaber2).
namespace itemFlag {
inline constexpr int Character = 2, Saber = 4, Saber2 = 8;
}

struct Menu;

struct Item {
    Window window;
    Rect textRect;
    int type = TypeText, alignment = 0, textAlignment = 0;
    float textAlignX = 0, textAlignY = 0, text2AlignX = 0, text2AlignY = 0;
    float textScale = 0.55f;
    int textStyle = 0;
    std::string text, text2, descText;
    std::string mouseEnterText, mouseExitText, mouseEnter, mouseExit, action, accept, selectionNext,
        selectionPrev, onFocus, leaveFocus;
    std::string cvar, cvarTest, enableCvar;
    int cvarFlags = 0;
    std::string focusSound, focusForce;
    std::vector<ColorRange> colorRanges;
    float special = 0;
    int cursorPos = 0, appearanceSlot = 0, value = 0;
    int font = 0;                       // the menu's font number, 1-based
    int invertYesNo = 0, xoffset = 0;
    int itemFlags = 0;
    std::string assetModel, assetShader;
    bool disabled = false;
    // Type data, the part the item's type uses.
    std::unique_ptr<ListBox> listBox;
    std::unique_ptr<EditField> edit;
    std::unique_ptr<Multi> multi;
    std::unique_ptr<Model> model;
    std::unique_ptr<TextScroll> scroll;
    Menu *parent = nullptr;             // the menu it belongs to
};

struct Menu {
    Window window;
    std::string font;
    bool fullScreen = false;
    int cursorItem = -1, fadeCycle = 0;
    float fadeClamp = 0, fadeAmount = 0;
    std::string onOpen, onClose, onAccept, onESC, soundName;
    Color focusColor{}, disableColor{};
    std::vector<std::unique_ptr<Item>> items;
    float appearanceTime = 0, appearanceIncrement = 0;
    int appearanceCount = 0;
    int descX = 0, descY = 0, descAlignment = 0, descTextStyle = 0;
    Color descColor{};
    float descScale = 0;
    std::string file;
};

struct FontAsset { std::string name; int pointSize = 0; };

// assetGlobalDef: what the engines read into their shared assets.
struct Assets {
    std::vector<FontAsset> fonts;       // in the order registered: the menus' font numbers
    std::string mediumFont;             // the font "mediumFont" named last
    std::string cursor, gradientBar, stripedFile;
    std::string menuEnterSound, menuExitSound, itemFocusSound, menuBuzzSound, itemFocusForce;
    std::string forceChosenSound, forceUnchosenSound;
    std::array<std::string, 2> datapadMoveSounds{};       // roll, jump
    std::array<std::string, 6> datapadSaberSounds{};
    std::vector<std::string> precacheSounds;
    float fadeClamp = 0, fadeAmount = 0;
    int fadeCycle = 0;
    float shadowX = 0, shadowY = 0;
    Color shadowColor{};
    Color focusColor{};
};

/* ------------------------------------------------------------------------
 * Keyword sets, in the retail tables' order
 * ---------------------------------------------------------------------- */

inline constexpr const char *kAcademyMenuKeywords[] = {"appearanceIncrement", "backcolor", "background", "border",
    "bordercolor", "borderSize", "cinematic", "descAlignment", "descTextStyle", "desccolor", "descX", "descY",
    "descScale", "disablecolor", "fadeClamp", "fadeCycle", "fadeAmount", "focuscolor", "font", "forecolor",
    "fullscreen", "itemDef", "name", "onClose", "onAccept", "onESC", "onOpen", "outlinecolor", "outOfBoundsClick",
    "ownerdraw", "ownerdrawFlag", "popup", "rect", "soundLoop", "style", "visible", "ignoreescape"};
inline constexpr const char *kOutcastMenuKeywords[] = {"appearanceIncrement", "backcolor", "background", "border",
    "bordercolor", "borderSize", "cinematic", "descAlignment", "desccolor", "descX", "descY", "descScale",
    "disablecolor", "fadeClamp", "fadeCycle", "fadeAmount", "focuscolor", "font", "forecolor", "fullscreen",
    "itemDef", "name", "onClose", "onESC", "onOpen", "outlinecolor", "outOfBoundsClick", "ownerdraw",
    "ownerdrawFlag", "popup", "rect", "soundLoop", "style", "visible"};
inline constexpr const char *kAcademyCommands[] = {"activate", "close", "exec", "fadein", "fadeout", "hide", "open",
    "openGoToMenu", "orbit", "play", "playVoice", "stopVoice", "ffplay", "setasset", "setbackground", "setcolor",
    "setcvar", "setfocus", "setitemcolor", "setitemflag", "show", "showMenu", "transition", "transition2",
    "setitembackground", "setitemtext", "setitemrect", "defer", "rundeferred", "delay", "transition3",
    "incrementfeeder", "decrementfeeder"};
inline constexpr const char *kOutcastCommands[] = {"activate", "close", "exec", "fadein", "fadeout", "hide", "open",
    "openGoToMenu", "orbit", "play", "playlooped", "setasset", "setbackground", "setcolor", "setcvar", "setfocus",
    "setitemcolor", "setitemflag", "show", "showMenu", "transition", "defer", "rundeferred"};
inline constexpr const char *kAcademyItemKeywords[] = {"accept", "selectNext", "selectPrev", "action",
    "addColorRange", "align", "appearance_slot", "asset_model", "asset_shader", "isCharacter", "isSaber",
    "isSaber2", "autowrapped", "backcolor", "background", "border", "bordercolor", "bordersize", "cinematic",
    "columns", "cvar", "cvarFloat", "cvarFloatList", "cvarSubString", "cvarStrList", "cvarTest", "decoration",
    "desctext", "disableCvar", "doubleclick", "elementheight", "elementtype", "elementwidth", "enableCvar",
    "feeder", "flag", "focusSound", "focusForce", "font", "forecolor", "group", "hideCvar", "horizontalscroll",
    "leaveFocus", "maxChars", "maxPaintChars", "model_angle", "model_fovx", "model_fovy", "model_origin",
    "model_rotation", "model_g2mins", "model_g2maxs", "model_g2skin", "model_g2anim", "mouseEnter",
    "mouseEnterText", "mouseExit", "mouseExitText", "name", "notselectable", "scrollhidden", "onFocus",
    "outlinecolor", "ownerdraw", "ownerdrawFlag", "rect", "showCvar", "special", "style", "text", "text2",
    "text2alignx", "text2aligny", "textalign", "textalignx", "textaligny", "textscale", "textstyle", "type",
    "visible", "wrapped", "invertyesno", "xoffset", "lineHeight"};
inline constexpr const char *kOutcastItemKeywords[] = {"action", "addColorRange", "align", "appearance_slot",
    "asset_model", "asset_shader", "autowrapped", "backcolor", "background", "border", "bordercolor", "bordersize",
    "cinematic", "columns", "cvar", "cvarFloat", "cvarFloatList", "cvarStrList", "cvarTest", "decoration",
    "desctext", "disableCvar", "doubleclick", "elementheight", "elementtype", "elementwidth", "enableCvar",
    "feeder", "flag", "focusSound", "font", "forecolor", "group", "hideCvar", "horizontalscroll", "leaveFocus",
    "maxChars", "maxPaintChars", "model_angle", "model_fovx", "model_fovy", "model_origin", "model_rotation",
    "mouseEnter", "mouseEnterText", "mouseExit", "mouseExitText", "name", "notselectable", "onFocus",
    "outlinecolor", "ownerdraw", "ownerdrawFlag", "rect", "showCvar", "special", "style", "text", "text2",
    "text2alignx", "text2aligny", "textalign", "textalignx", "textaligny", "textscale", "textstyle", "type",
    "visible", "wrapped"};

template<std::size_t N>
inline bool Has(const char *const (&list)[N], std::string_view word) {
    for (const char *entry : list) {
        if (word.size() == std::strlen(entry)) {
            bool same = true;
            for (std::size_t n = 0; n < word.size() && same; ++n)
                same = std::tolower(static_cast<unsigned char>(word[n])) ==
                       std::tolower(static_cast<unsigned char>(entry[n]));
            if (same) return true;
        }
    }
    return false;
}

inline bool Same(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t n = 0; n < a.size(); ++n)
        if (std::tolower(static_cast<unsigned char>(a[n])) != std::tolower(static_cast<unsigned char>(b[n])))
            return false;
    return true;
}

inline bool IsMenuKeyword(Dialect d, std::string_view w) {
    return d == Dialect::Academy ? Has(kAcademyMenuKeywords, w) : Has(kOutcastMenuKeywords, w);
}
inline bool IsItemKeyword(Dialect d, std::string_view w) {
    return d == Dialect::Academy ? Has(kAcademyItemKeywords, w) : Has(kOutcastItemKeywords, w);
}
inline bool IsCommand(Dialect d, std::string_view w) {
    return d == Dialect::Academy ? Has(kAcademyCommands, w) : Has(kOutcastCommands, w);
}

/* ------------------------------------------------------------------------
 * The reader
 * ---------------------------------------------------------------------- */

struct Diagnostic { std::string file; int line = 0; std::string message; };

class Reader {
public:
    using Warn = std::function<void(const Diagnostic &)>;

    Reader(Dialect dialect, Warn warn) : dialect_(dialect), warn_(std::move(warn)) {}

    Dialect dialect() const { return dialect_; }

    /* UI_ParseMenu: one menu file's assetGlobalDef and menuDef blocks, into
     * assets and menus. Returns false only when the text could not be read
     * as a menu file at all. */
    bool File(const std::string &name, std::string_view text, Assets &assets,
              std::vector<std::unique_ptr<Menu>> &menus) {
        Tokens tokens(text);
        file_ = name;
        tokens_ = &tokens;
        for (;;) {
            const std::string token = tokens.Next(true);
            if (token.empty()) break;
            if (token[0] == '{') continue;
            if (token[0] == '}') break;
            if (Same(token, "assetGlobalDef")) {
                if (AssetBlock(assets)) continue;
                break;
            }
            if (Same(token, "menudef")) {
                auto menu = std::make_unique<Menu>();
                InitMenu(*menu, assets);
                menu->file = name;
                if (MenuBlock(*menu)) {
                    PostParse(*menu);
                    menus.push_back(std::move(menu));
                }
                continue;
            }
            Warning("Invalid keyword '" + token + "'");
        }
        tokens_ = nullptr;
        return true;
    }

    /* UI_LoadMenus' list file: `{ loadMenu { "file" ... } ... }`, the names
     * in order. A loadMenu block that does not open or close stops the list,
     * as the engines' Load_Menu does. */
    static std::vector<std::string> MenuList(std::string_view text, const Warn &warn, const std::string &file) {
        std::vector<std::string> names;
        Tokens tokens(text);
        for (;;) {
            std::string token = tokens.Next(true);
            if (token.empty() || token[0] == '}') break;
            if (token[0] == '{') continue;
            if (!Same(token, "loadmenu")) {
                if (warn) warn({file, tokens.Line(), "Unknown keyword '" + token + "' in menus file"});
                continue;
            }
            token = tokens.Next(true);
            if (token.empty() || token[0] != '{') break;
            bool closed = false;
            for (;;) {
                token = tokens.Next(true);
                if (token.empty()) break;
                if (token[0] == '}') { closed = true; break; }
                names.push_back(token);
            }
            if (!closed) break;
        }
        return names;
    }

    // PC_Script_Parse's form of a script: what follows its keyword.
    bool Script(std::string &out) {
        std::string token = tokens_->Next(true);
        if (token.empty() || token[0] != '{') return false;
        std::string script;
        for (;;) {
            token = tokens_->Next(true);
            if (token.empty()) { if (tokens_->Ended()) return false; continue; }
            if (token[0] == '}') { out = script; return true; }
            if (token.size() > 1) script += '"' + token + '"';
            else script += token;
            script += ' ';
            if (script.size() > kMaxScript) script.resize(kMaxScript);
        }
    }

    static constexpr std::size_t kMaxScript = 4095;   // PC_Script_Parse's buffer, less its end

private:
    void Warning(const std::string &message) {
        if (warn_) warn_({file_, tokens_ ? tokens_->Line() : 0, message});
    }

    bool String(std::string &out) { return tokens_->String(out); }
    bool Int(int &out) { return tokens_->Int(out); }
    bool Float(float &out) { return tokens_->Float(out); }
    bool ColorValue(Color &out) {
        for (float &value : out) if (!Float(value)) return false;
        return true;
    }
    bool RectValue(Rect &out) { return Float(out.x) && Float(out.y) && Float(out.w) && Float(out.h); }
    bool Vec(Vec3 &out) { return Float(out[0]) && Float(out[1]) && Float(out[2]); }

    template<std::size_t N>
    bool Named(const char *const (&list)[N], int count, int &value, const char *what) {
        std::string word;
        if (!String(word)) return false;
        for (int n = 0; n < count && n < static_cast<int>(N); ++n) {
            if (Same(word, list[n])) { value = n; return true; }
        }
        Warning(std::string("Unknown ") + what + " value '" + word + "'");
        return true;
    }

    static void InitMenu(Menu &menu, const Assets &assets) {
        menu.fadeAmount = assets.fadeAmount;
        menu.fadeClamp = assets.fadeClamp;
        menu.fadeCycle = assets.fadeCycle;
    }

    // Asset_Parse: each engine's own keywords; others warn and are skipped.
    bool AssetBlock(Assets &assets) {
        std::string token = tokens_->Next(true);
        if (token.empty() || token[0] != '{') return false;
        const bool academy = dialect_ == Dialect::Academy;
        for (;;) {
            token = tokens_->Next(true);
            if (token.empty()) { if (tokens_->Ended()) return false; continue; }
            if (token[0] == '}') return true;
            std::string value;
            if (Same(token, "smallFont") || Same(token, "mediumFont") || Same(token, "bigFont")) {
                if (!String(value)) { Warning("Bad 1st parameter for keyword '" + token + "'"); return false; }
                int size = 0;
                Int(size);
                assets.fonts.push_back({value, size});
                if (Same(token, "mediumFont")) assets.mediumFont = value;
                continue;
            }
            if (!academy && Same(token, "stripedFile")) {
                if (!String(value)) { Warning("Bad 1st parameter for keyword 'stripedFile'"); return false; }
                assets.stripedFile = value;
                continue;
            }
            struct Sound { const char *name; std::string *target; bool academyOnly; };
            const Sound sounds[] = {
                {"gradientbar", &assets.gradientBar, false}, {"menuEnterSound", &assets.menuEnterSound, false},
                {"menuExitSound", &assets.menuExitSound, false}, {"itemFocusSound", &assets.itemFocusSound, false},
                {"itemFocusForce", &assets.itemFocusForce, true}, {"menuBuzzSound", &assets.menuBuzzSound, false},
                {"forceChosenSound", &assets.forceChosenSound, true},
                {"forceUnchosenSound", &assets.forceUnchosenSound, true},
                {"datapadmoveRollSound", &assets.datapadMoveSounds[0], true},
                {"datapadmoveJumpSound", &assets.datapadMoveSounds[1], true},
                {"datapadmoveSaberSound1", &assets.datapadSaberSounds[0], true},
                {"datapadmoveSaberSound2", &assets.datapadSaberSounds[1], true},
                {"datapadmoveSaberSound3", &assets.datapadSaberSounds[2], true},
                {"datapadmoveSaberSound4", &assets.datapadSaberSounds[3], true},
                {"datapadmoveSaberSound5", &assets.datapadSaberSounds[4], true},
                {"datapadmoveSaberSound6", &assets.datapadSaberSounds[5], true},
                {"cursor", &assets.cursor, false},
            };
            bool done = false;
            for (const Sound &sound : sounds) {
                if ((sound.academyOnly && !academy) || !Same(token, sound.name)) continue;
                if (!String(value)) return false;
                *sound.target = value;
                done = true;
                break;
            }
            if (done) continue;
            if (Same(token, "fadeClamp")) { if (!Float(assets.fadeClamp)) return false; continue; }
            if (Same(token, "fadeCycle")) { if (!Int(assets.fadeCycle)) return false; continue; }
            if (Same(token, "fadeAmount")) { if (!Float(assets.fadeAmount)) return false; continue; }
            if (Same(token, "shadowX")) { if (!Float(assets.shadowX)) return false; continue; }
            if (Same(token, "shadowY")) { if (!Float(assets.shadowY)) return false; continue; }
            if (Same(token, "shadowColor")) { if (!ColorValue(assets.shadowColor)) return false; continue; }
            if (Same(token, "focuscolor")) { if (!ColorValue(assets.focusColor)) return false; continue; }
            if (academy && Same(token, "precacheSound")) {
                if (!Script(value)) return false;
                Tokens list(value);
                for (std::string name = list.Next(false); !list.Ended() || !name.empty(); name = list.Next(false)) {
                    if (!name.empty() && name != ";") assets.precacheSounds.push_back(name);
                    if (list.Ended()) break;
                }
                continue;
            }
            Warning("Unknown asset keyword '" + token + "'");
        }
    }

    bool MenuBlock(Menu &menu) {
        std::string token = tokens_->Next(true);
        if (token.empty() || token[0] != '{') { Warning("Misplaced {"); return false; }
        for (;;) {
            token = tokens_->Next(true);
            if (token.empty()) {
                if (tokens_->Ended()) { Warning("End of file inside menu."); return false; }
                continue;
            }
            if (token[0] == '}') return true;
            if (!IsMenuKeyword(dialect_, token)) {
                Warning("Unknown menu keyword " + token);
                continue;
            }
            if (!MenuKeyword(menu, token)) {
                Warning("Couldn't parse menu keyword " + token);
                return false;
            }
        }
    }

    bool MenuKeyword(Menu &menu, const std::string &key) {
        Window &w = menu.window;
        int value = 0;
        if (Same(key, "appearanceIncrement")) return Float(menu.appearanceIncrement);
        if (Same(key, "backcolor")) return ColorValue(w.backColor);
        if (Same(key, "background")) return String(w.background);
        if (Same(key, "border")) return Int(w.border);
        if (Same(key, "bordercolor")) return ColorValue(w.borderColor);
        if (Same(key, "borderSize")) return Float(w.borderSize);
        if (Same(key, "cinematic")) return String(w.cinematicName);
        if (Same(key, "descAlignment")) return Named(kAligns, 3, menu.descAlignment, "desc alignment");
        if (Same(key, "descTextStyle")) return Int(menu.descTextStyle);
        if (Same(key, "desccolor")) return ColorValue(menu.descColor);
        if (Same(key, "descX")) return Int(menu.descX);
        if (Same(key, "descY")) return Int(menu.descY);
        if (Same(key, "descScale")) return Float(menu.descScale);
        if (Same(key, "disablecolor")) return ColorValue(menu.disableColor);
        if (Same(key, "fadeClamp")) return Float(menu.fadeClamp);
        if (Same(key, "fadeCycle")) return Int(menu.fadeCycle);
        if (Same(key, "fadeAmount")) return Float(menu.fadeAmount);
        if (Same(key, "focuscolor")) return ColorValue(menu.focusColor);
        if (Same(key, "font")) return String(menu.font);
        if (Same(key, "forecolor")) return ForeColor(w);
        if (Same(key, "fullscreen")) {
            if (!Int(value)) return false;
            menu.fullScreen = value != 0;
            return true;
        }
        if (Same(key, "itemDef")) return ItemDef(menu);
        if (Same(key, "name")) return String(w.name);
        if (Same(key, "onClose")) return Script(menu.onClose);
        if (Same(key, "onAccept")) return Script(menu.onAccept);
        if (Same(key, "onESC")) return Script(menu.onESC);
        if (Same(key, "onOpen")) return Script(menu.onOpen);
        if (Same(key, "outlinecolor")) return ColorValue(w.outlineColor);
        if (Same(key, "outOfBoundsClick")) { w.flags |= flag::OutOfBoundsClick; return true; }
        if (Same(key, "ownerdraw")) return Int(w.ownerDraw);
        if (Same(key, "ownerdrawFlag")) {
            if (!Int(value)) return false;
            w.ownerDrawFlags |= value;
            return true;
        }
        if (Same(key, "popup")) { w.flags |= flag::Popup; return true; }
        if (Same(key, "rect")) return RectValue(w.rect);
        if (Same(key, "soundLoop")) return String(menu.soundName);
        if (Same(key, "style")) return Named(kStyles, 6, w.style, "menu style");
        if (Same(key, "visible") || Same(key, "ignoreescape")) {
            if (!Int(value)) return false;
            if (value) w.flags |= Same(key, "visible") ? flag::Visible : flag::IgnoreEscape;
            return true;
        }
        return false;
    }

    bool ForeColor(Window &w) {
        for (float &channel : w.foreColor) {
            float value = 0;
            if (!Float(value)) return false;
            if (value < 0) { w.flags |= flag::PlayerColor; return true; }   // the player's colour
            channel = value;
            w.flags |= flag::ForeColorSet;
        }
        return true;
    }

    bool ItemDef(Menu &menu) {
        if (menu.items.size() >= kMaxItems) {
            Warning("Exceeded item/menu max of " + std::to_string(kMaxItems));
            return true;
        }
        auto item = std::make_unique<Item>();
        item->parent = &menu;
        std::string token;
        if (!String(token) || token[0] != '{') return false;
        for (;;) {
            if (!String(token)) { Warning("End of file inside menu item"); return false; }
            if (token[0] == '}') break;
            if (!IsItemKeyword(dialect_, token)) {
                Warning("Unknown item keyword '" + token + "'");
                continue;
            }
            if (!ItemKeyword(*item, token)) {
                Warning("Couldn't parse item keyword '" + token + "'");
                return false;
            }
        }
        if (item->type == TypeListBox && item->listBox) {
            item->listBox->cursorPos = item->listBox->startPos = item->listBox->endPos = 0;
        }
        item->cursorPos = 0;
        menu.items.push_back(std::move(item));
        return true;
    }

    // Item_ValidateTypeData: the data the item's type keeps, made once.
    static void TypeData(Item &item) {
        switch (item.type) {
        case TypeListBox: if (!item.listBox) item.listBox = std::make_unique<ListBox>(); break;
        case TypeEditField: case TypeNumericField: case TypeYesNo: case TypeBind: case TypeSlider: case TypeText:
            if (!item.edit) {
                item.edit = std::make_unique<EditField>();
                if (item.type == TypeEditField && !item.edit->maxPaintChars) item.edit->maxPaintChars = kMaxEditField;
            }
            break;
        case TypeMulti: if (!item.multi) item.multi = std::make_unique<Multi>(); break;
        case TypeModel: if (!item.model) item.model = std::make_unique<Model>(); break;
        case TypeTextScroll: if (!item.scroll) item.scroll = std::make_unique<TextScroll>(); break;
        default: break;
        }
    }

    /* cvarStrList: text and value pairs; cvarFloatList: text then a number on
     * its line. Commas and semicolons between entries are skipped, and a list
     * that reaches the limit stops the item, as the engines' does. A string
     * list may instead name `feeder`: the item's feeder (species, languages)
     * fills it when the menus run. */
    bool CvarList(Item &item, bool strings) {
        TypeData(item);
        if (!item.multi) return false;   // only a multi keeps a list
        Multi &multi = *item.multi;
        multi.cvarList.clear(); multi.cvarStr.clear(); multi.cvarValue.clear();
        multi.strDef = strings;
        multi.listFeeder = false;
        std::string token;
        if (!String(token)) return false;
        if (strings && Same(token, "feeder")) {
            multi.listFeeder = true;
            return true;
        }
        if (token[0] != '{') return false;
        bool second = false;
        for (;;) {
            if (!String(token)) { Warning("end of file inside menu item"); return false; }
            if (token[0] == '}') return true;
            if (token[0] == ',' || token[0] == ';') continue;
            if (strings) {
                if (!second) { multi.cvarList.push_back(token); second = true; continue; }
                multi.cvarStr.push_back(token);
                second = false;
            } else {
                float value = 0;
                multi.cvarList.push_back(token);
                if (!Float(value)) return false;
                multi.cvarValue.push_back(value);
            }
            if (multi.cvarList.size() >= kMaxMulti) return false;
        }
    }

    bool ItemKeyword(Item &item, const std::string &key) {
        Window &w = item.window;
        int value = 0;
        auto script = [&](std::string &out) { return Script(out); };
        if (Same(key, "accept")) return script(item.accept);
        if (Same(key, "selectNext")) return script(item.selectionNext);
        if (Same(key, "selectPrev")) return script(item.selectionPrev);
        if (Same(key, "action")) return script(item.action);
        if (Same(key, "addColorRange")) {
            ColorRange range;
            if (!Float(range.low) || !Float(range.high) || !ColorValue(range.color)) return false;
            if (item.colorRanges.size() < kMaxColorRanges) item.colorRanges.push_back(range);
            return true;
        }
        if (Same(key, "align")) return Int(item.alignment);
        if (Same(key, "appearance_slot")) return Int(item.appearanceSlot);
        if (Same(key, "asset_model")) { TypeData(item); return String(item.assetModel); }
        if (Same(key, "asset_shader")) return String(item.assetShader);
        if (Same(key, "isCharacter") || Same(key, "isSaber") || Same(key, "isSaber2")) {
            if (!Int(value)) return false;
            if (value) item.itemFlags |= Same(key, "isCharacter") ? itemFlag::Character
                : Same(key, "isSaber") ? itemFlag::Saber : itemFlag::Saber2;
            return true;
        }
        if (Same(key, "autowrapped")) { w.flags |= flag::AutoWrapped; return true; }
        if (Same(key, "backcolor")) {
            if (!ColorValue(w.backColor)) return false;
            w.flags |= flag::BackColorSet;
            return true;
        }
        if (Same(key, "background")) return String(w.background);
        if (Same(key, "border")) return Int(w.border);
        if (Same(key, "bordercolor")) return ColorValue(w.borderColor);
        if (Same(key, "bordersize")) return Float(w.borderSize);
        if (Same(key, "cinematic")) return String(w.cinematicName);
        if (Same(key, "columns")) {
            TypeData(item);
            if (!item.listBox) return false;
            int count = 0;
            if (!Int(count)) return true;   // the engines ignore a missing count
            if (count > 16) count = 16;
            item.listBox->numColumns = count;
            for (int n = 0; n < count; ++n) {
                ColumnInfo info;
                if (!Int(info.pos) || !Int(info.width) || !Int(info.maxChars)) return false;
                item.listBox->columns[n] = info;
            }
            return true;
        }
        if (Same(key, "cvar")) {
            if (!String(item.cvar)) return false;
            TypeData(item);
            if (item.edit) item.edit->minVal = item.edit->maxVal = item.edit->defVal = -1;
            return true;
        }
        if (Same(key, "cvarFloat")) {
            TypeData(item);
            if (!item.edit) return false;
            return String(item.cvar) && Float(item.edit->defVal) && Float(item.edit->minVal) &&
                   Float(item.edit->maxVal);
        }
        if (Same(key, "cvarFloatList")) return CvarList(item, false);
        if (Same(key, "cvarStrList")) return CvarList(item, true);
        if (Same(key, "cvarSubString")) { item.cvarFlags |= cvarRule::Substring; return true; }
        if (Same(key, "cvarTest")) return String(item.cvarTest);
        if (Same(key, "decoration")) { w.flags |= flag::Decoration; return true; }
        if (Same(key, "desctext")) return String(item.descText);
        if (Same(key, "enableCvar") || Same(key, "disableCvar") || Same(key, "showCvar") || Same(key, "hideCvar")) {
            if (!script(item.enableCvar)) return false;
            item.cvarFlags = Same(key, "enableCvar") ? cvarRule::Enable : Same(key, "disableCvar") ? cvarRule::Disable
                : Same(key, "showCvar") ? cvarRule::Show : cvarRule::Hide;
            return true;
        }
        if (Same(key, "doubleclick")) {
            TypeData(item);
            if (!item.listBox) return false;
            return script(item.listBox->doubleClick);
        }
        if (Same(key, "elementheight") || Same(key, "elementwidth")) {
            TypeData(item);
            if (!item.listBox) return false;
            return Float(Same(key, "elementheight") ? item.listBox->elementHeight : item.listBox->elementWidth);
        }
        if (Same(key, "elementtype")) {
            TypeData(item);
            if (!item.listBox) return false;
            return Int(item.listBox->elementStyle);
        }
        if (Same(key, "feeder")) return Float(item.special);
        if (Same(key, "flag")) {
            std::string word;
            if (!String(word)) return false;
            if (Same(word, "WINDOW_INACTIVE")) w.flags |= flag::Inactive;
            else Warning("Unknown item flag value '" + word + "'");
            return true;
        }
        if (Same(key, "focusSound")) return String(item.focusSound);
        if (Same(key, "focusForce")) return String(item.focusForce);
        if (Same(key, "font")) return Int(item.font);
        if (Same(key, "forecolor")) return ForeColor(w);
        if (Same(key, "group")) return String(w.group);
        if (Same(key, "horizontalscroll")) { w.flags |= flag::Horizontal; return true; }
        if (Same(key, "leaveFocus")) return script(item.leaveFocus);
        if (Same(key, "maxChars") || Same(key, "maxPaintChars")) {
            TypeData(item);
            if (!item.edit) return false;
            return Int(Same(key, "maxChars") ? item.edit->maxChars : item.edit->maxPaintChars);
        }
        if (Same(key, "lineHeight")) {
            TypeData(item);
            if (!item.scroll) return false;
            return Int(item.scroll->lineHeight);
        }
        if (Same(key, "model_angle") || Same(key, "model_rotation")) {
            TypeData(item);
            if (!item.model) return false;
            return Int(Same(key, "model_angle") ? item.model->angle : item.model->rotationSpeed);
        }
        if (Same(key, "model_fovx") || Same(key, "model_fovy")) {
            TypeData(item);
            if (!item.model) return false;
            return Float(Same(key, "model_fovx") ? item.model->fovX : item.model->fovY);
        }
        if (Same(key, "model_origin") || Same(key, "model_g2mins") || Same(key, "model_g2maxs")) {
            TypeData(item);
            if (!item.model) return false;
            return Vec(Same(key, "model_origin") ? item.model->origin
                       : Same(key, "model_g2mins") ? item.model->g2mins : item.model->g2maxs);
        }
        if (Same(key, "model_g2skin") || Same(key, "model_g2anim")) {
            TypeData(item);
            if (!item.model) return false;
            return String(Same(key, "model_g2skin") ? item.model->g2skin : item.model->g2anim);
        }
        if (Same(key, "mouseEnter")) return script(item.mouseEnter);
        if (Same(key, "mouseEnterText")) return script(item.mouseEnterText);
        if (Same(key, "mouseExit")) return script(item.mouseExit);
        if (Same(key, "mouseExitText")) return script(item.mouseExitText);
        if (Same(key, "name")) return String(w.name);
        if (Same(key, "notselectable") || Same(key, "scrollhidden")) {
            TypeData(item);
            if (item.type == TypeListBox && item.listBox)
                (Same(key, "notselectable") ? item.listBox->notSelectable : item.listBox->scrollHidden) = true;
            return true;
        }
        if (Same(key, "onFocus")) return script(item.onFocus);
        if (Same(key, "outlinecolor")) return ColorValue(w.outlineColor);
        if (Same(key, "ownerdraw")) {
            if (!Int(w.ownerDraw)) return false;
            item.type = TypeOwnerDraw;
            return true;
        }
        if (Same(key, "ownerdrawFlag")) {
            if (!Int(value)) return false;
            w.ownerDrawFlags |= value;
            return true;
        }
        if (Same(key, "rect")) return RectValue(w.rectClient);
        if (Same(key, "special")) return Float(item.special);
        if (Same(key, "style")) return Named(kStyles, 6, w.style, "item style");
        if (Same(key, "text")) return String(item.text);
        if (Same(key, "text2")) return String(item.text2);
        if (Same(key, "text2alignx")) return Float(item.text2AlignX);
        if (Same(key, "text2aligny")) return Float(item.text2AlignY);
        if (Same(key, "textalign")) return Named(kAligns, 3, item.textAlignment, "text alignment");
        if (Same(key, "textalignx")) return Float(item.textAlignX);
        if (Same(key, "textaligny")) return Float(item.textAlignY);
        if (Same(key, "textscale")) return Float(item.textScale);
        if (Same(key, "textstyle")) return Int(item.textStyle);
        if (Same(key, "type")) {
            const int before = item.type;
            if (!Named(kTypes, TypeCount(dialect_), item.type, "item type")) return false;
            if (item.type != before || item.type == TypeText) TypeData(item);
            return true;
        }
        if (Same(key, "visible")) {
            if (!Int(value)) return false;
            if (value) w.flags |= flag::Visible;
            return true;
        }
        if (Same(key, "wrapped")) { w.flags |= flag::Wrapped; return true; }
        if (Same(key, "invertyesno")) return Int(item.invertYesNo);
        if (Same(key, "xoffset")) return Int(item.xoffset);
        return false;
    }

    // Menu_PostParse: a full-screen menu covers the virtual screen.
    static void PostParse(Menu &menu) {
        if (menu.fullScreen) menu.window.rect = {0, 0, 640, 480};
        UpdatePosition(menu);
    }

public:
    // Menu_UpdatePosition: every item placed within its menu (and border).
    static void UpdatePosition(Menu &menu) {
        float x = menu.window.rect.x, y = menu.window.rect.y;
        if (menu.window.border) { x += menu.window.borderSize; y += menu.window.borderSize; }
        for (auto &item : menu.items) SetScreenCoords(*item, x, y);
    }

    // Item_SetScreenCoords.
    static void SetScreenCoords(Item &item, float x, float y) {
        if (item.window.border) { x += item.window.borderSize; y += item.window.borderSize; }
        item.window.rect = {x + item.window.rectClient.x, y + item.window.rectClient.y,
                            item.window.rectClient.w, item.window.rectClient.h};
        item.textRect.w = 0;   // its text is measured again
    }

    static constexpr std::size_t kMaxItems = 150;       // MAX_MENUITEMS
    static constexpr std::size_t kMaxColorRanges = 10;  // MAX_COLOR_RANGES
    static constexpr std::size_t kMaxMulti = 64;        // MAX_MULTI_CVARS
    static constexpr int kMaxEditField = 256;           // MAX_EDITFIELD

private:
    Dialect dialect_;
    Warn warn_;
    std::string file_;
    Tokens *tokens_ = nullptr;
};

} // namespace fntech3::raven::menu

#endif
