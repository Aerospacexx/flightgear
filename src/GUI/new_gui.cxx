// new_gui.cxx: implementation of XML-configurable GUI support.

#include "new_gui.hxx"

#include <plib/pu.h>
#include <plib/ul.h>

#include <simgear/compiler.h>
#include <simgear/structure/exception.hxx>

#include <Main/fg_props.hxx>

#include "menubar.hxx"
#include "dialog.hxx"

extern puFont FONT_HELVETICA_14;
extern puFont FONT_SANS_12B;




////////////////////////////////////////////////////////////////////////
// Implementation of NewGUI.
////////////////////////////////////////////////////////////////////////



NewGUI::NewGUI ()
    : _menubar(new FGMenuBar),
      _active_dialog(0)
{
}

NewGUI::~NewGUI ()
{
    delete _menubar;
    _dialog_props.clear();
    for (_itt_t it = _colors.begin(); it != _colors.end(); ++it)
        delete it->second;
}

void
NewGUI::init ()
{
    setStyle();
    char path1[1024];
    char path2[1024];
    ulMakePath(path1, globals->get_fg_root().c_str(), "gui");
    ulMakePath(path2, path1, "dialogs");
    readDir(path2);
    _menubar->init();
}

void
NewGUI::reinit ()
{
    reset(true);
    fgSetBool("/sim/signals/reinit-gui", true);
}

void
NewGUI::redraw ()
{
    reset(false);
}

void
NewGUI::reset (bool reload)
{
    map<string,FGDialog *>::iterator iter;
    vector<string> dlg;
    // close all open dialogs and remember them ...
    for (iter = _active_dialogs.begin(); iter != _active_dialogs.end(); ++iter)
        dlg.push_back(iter->first);

    unsigned int i;
    for (i = 0; i < dlg.size(); i++)
        closeDialog(dlg[i]);

    setStyle();

    unbind();
    delete _menubar;
    _menubar = new FGMenuBar;

    if (reload) {
        _dialog_props.clear();
        init();
    } else {
        _menubar->init();
    }

    bind();

    // open dialogs again
    for (i = 0; i < dlg.size(); i++)
        showDialog(dlg[i]);
}

void
NewGUI::bind ()
{
    fgTie("/sim/menubar/visibility", this,
          &NewGUI::getMenuBarVisible, &NewGUI::setMenuBarVisible);
}

void
NewGUI::unbind ()
{
    fgUntie("/sim/menubar/visibility");
}

void
NewGUI::update (double delta_time_sec)
{
    map<string,FGDialog *>::iterator iter = _active_dialogs.begin();
    for(/**/; iter != _active_dialogs.end(); iter++)
        iter->second->update();
}

bool
NewGUI::showDialog (const string &name)
{
    if (_dialog_props.find(name) == _dialog_props.end()) {
        SG_LOG(SG_GENERAL, SG_ALERT, "Dialog " << name << " not defined");
        return false;
    } else {
        if(!_active_dialogs[name])
            _active_dialogs[name] = new FGDialog(_dialog_props[name]);
        return true;
    }
}

bool
NewGUI::closeActiveDialog ()
{
    if (_active_dialog == 0)
        return false;

    // Kill any entries in _active_dialogs...  Is there an STL
    // algorithm to do (delete map entries by value, not key)?  I hate
    // the STL :) -Andy
    map<string,FGDialog *>::iterator iter = _active_dialogs.begin();
    for(/**/; iter != _active_dialogs.end(); iter++) {
        if(iter->second == _active_dialog) {
            _active_dialogs.erase(iter);
            // iter is no longer valid
            break;
        }
    }

    delete _active_dialog;
    _active_dialog = 0;
    return true;
}

bool
NewGUI::closeDialog (const string& name)
{
    if(_active_dialogs.find(name) != _active_dialogs.end()) {
        if(_active_dialog == _active_dialogs[name])
            _active_dialog = 0;
        delete _active_dialogs[name];
        _active_dialogs.erase(name);
        return true;
    }
    return false; // dialog wasn't open...
}

SGPropertyNode_ptr
NewGUI::getDialogProperties (const string &name)
{
    if(_dialog_props.find(name) != _dialog_props.end())
        return _dialog_props[name];

    SG_LOG(SG_GENERAL, SG_DEBUG, "dialog '" << name << "' missing");
    return 0;
}

FGDialog *
NewGUI::getDialog (const string &name)
{
    if(_active_dialogs.find(name) != _active_dialogs.end())
        return _active_dialogs[name];

    SG_LOG(SG_GENERAL, SG_DEBUG, "dialog '" << name << "' missing");
    return 0;
}

void
NewGUI::setActiveDialog (FGDialog * dialog)
{
    _active_dialog = dialog;
}

FGDialog *
NewGUI::getActiveDialog ()
{
    return _active_dialog;
}

FGMenuBar *
NewGUI::getMenuBar ()
{
    return _menubar;
}

bool
NewGUI::getMenuBarVisible () const
{
    return _menubar->isVisible();
}

void
NewGUI::setMenuBarVisible (bool visible)
{
    if (visible)
        _menubar->show();
    else
        _menubar->hide();
}

static bool
test_extension (const char * path, const char * ext)
{
    int pathlen = strlen(path);
    int extlen = strlen(ext);

    for (int i = 1; i <= pathlen && i <= extlen; i++) {
        if (path[pathlen-i] != ext[extlen-i])
            return false;
    }
    return true;
}

void
NewGUI::newDialog (SGPropertyNode* props)
{
    const char* cname = props->getStringValue("name");
    if(!cname) {
        SG_LOG(SG_GENERAL, SG_ALERT, "New dialog has no <name> property");
        return;
    }
    string name = cname;
    if(_active_dialogs.find(name) == _active_dialogs.end())
        _dialog_props[name] = props;
}

void
NewGUI::readDir (const char * path)
{
    ulDir * dir = ulOpenDir(path);

    if (dir == 0) {
        SG_LOG(SG_GENERAL, SG_ALERT, "Failed to read GUI files from "
               << path);
        return;
    }

    for (ulDirEnt * dirEnt = ulReadDir(dir);
         dirEnt != 0;
         dirEnt = ulReadDir(dir)) {

        char subpath[1024];

        ulMakePath(subpath, path, dirEnt->d_name);

        if (!dirEnt->d_isdir && test_extension(subpath, ".xml")) {
            SGPropertyNode * props = new SGPropertyNode;
            try {
                readProperties(subpath, props);
            } catch (const sg_exception &) {
                SG_LOG(SG_INPUT, SG_ALERT, "Error parsing dialog "
                       << subpath);
                delete props;
                continue;
            }
            SGPropertyNode *nameprop = props->getNode("name");
            if (!nameprop) {
                SG_LOG(SG_INPUT, SG_WARN, "dialog " << subpath
                   << " has no name; skipping.");
                delete props;
                continue;
            }
            string name = nameprop->getStringValue();
            if (_dialog_props[name])
                delete (SGPropertyNode *)_dialog_props[name];

            _dialog_props[name] = props;
        }
    }
    ulCloseDir(dir);
}



////////////////////////////////////////////////////////////////////////
// Style handling.
////////////////////////////////////////////////////////////////////////

void
NewGUI::setStyle (void)
{
    _itt_t it;
    for (it = _colors.begin(); it != _colors.end(); ++it)
      delete it->second;
    _colors.clear();

    // set up the traditional colors as default
    _colors["background"] = new FGColor(0.8f, 0.8f, 0.9f, 0.85f);
    _colors["foreground"] = new FGColor(0.0f, 0.0f, 0.0f, 1.0f);
    _colors["highlight"]  = new FGColor(0.7f, 0.7f, 0.7f, 1.0f);
    _colors["label"]      = new FGColor(0.0f, 0.0f, 0.0f, 1.0f);
    _colors["legend"]     = new FGColor(0.0f, 0.0f, 0.0f, 1.0f);
    _colors["misc"]       = new FGColor(0.0f, 0.0f, 0.0f, 1.0f);
    _colors["inputfield"] = new FGColor(0.8f, 0.7f, 0.7f, 1.0f);

    //puSetDefaultStyle();

    int which = fgGetInt("/sim/gui/current-style", 0);
    SGPropertyNode *sim = globals->get_props()->getNode("sim/gui", true);
    SGPropertyNode *n = sim->getChild("style", which);
    if (!n)
        n = sim->getChild("style", 0, true);

    setupFont(n->getNode("fonts/gui", true));
    n = n->getNode("colors", true);

    for (int i = 0; i < n->nChildren(); i++) {
        SGPropertyNode *child = n->getChild(i);
        _colors[child->getName()] = new FGColor(child);
    }

    FGColor *c = _colors["background"];
    puSetDefaultColourScheme(c->red(), c->green(), c->blue(), c->alpha());
}


void
NewGUI::setupFont (SGPropertyNode *node)
{
    _font = globals->get_fontcache()->get(node);
    puSetDefaultFonts(*_font, *_font);
    return;
}




////////////////////////////////////////////////////////////////////////
// FGColor class.
////////////////////////////////////////////////////////////////////////

bool
FGColor::merge(const SGPropertyNode *node)
{
    if (!node)
        return false;

    bool dirty = false;
    const SGPropertyNode * n;
    if ((n = node->getNode("red")))
        _red = n->getFloatValue(), dirty = true;
    if ((n = node->getNode("green")))
        _green = n->getFloatValue(), dirty = true;
    if ((n = node->getNode("blue")))
        _blue = n->getFloatValue(), dirty = true;
    if ((n = node->getNode("alpha")))
        _alpha = n->getFloatValue(), dirty = true;
    return dirty;
}

bool
FGColor::merge(const FGColor *color)
{
    bool dirty = false;
    if (color && color->_red >= 0.0)
        _red = color->_red, dirty = true;
    if (color && color->_green >= 0.0)
        _green = color->_green, dirty = true;
    if (color && color->_blue >= 0.0)
        _blue = color->_blue, dirty = true;
    if (color && color->_alpha >= 0.0)
        _alpha = color->_alpha, dirty = true;
    return dirty;
}




////////////////////////////////////////////////////////////////////////
// FGFontCache class.
////////////////////////////////////////////////////////////////////////

static const struct {
    char *name;
    puFont *font;
} guifonts[] = {
    { "default",      &FONT_HELVETICA_14 },
    { "FIXED_8x13",   &PUFONT_8_BY_13 },
    { "FIXED_9x15",   &PUFONT_9_BY_15 },
    { "TIMES_10",     &PUFONT_TIMES_ROMAN_10 },
    { "TIMES_24",     &PUFONT_TIMES_ROMAN_24 },
    { "HELVETICA_10", &PUFONT_HELVETICA_10 },
    { "HELVETICA_12", &PUFONT_HELVETICA_12 },
    { "HELVETICA_14", &FONT_HELVETICA_14 },
    { "HELVETICA_18", &PUFONT_HELVETICA_18 },
    { "SANS_12B",     &FONT_SANS_12B },
    { 0, 0 }
};


FGFontCache::FGFontCache() :
    _initialized(false)
{
}

FGFontCache::~FGFontCache()
{
   map<const string, fnt *>::iterator it, end = _fonts.end();
   for (it = _fonts.begin(); it != end; ++it)
       delete it->second;
}

struct FGFontCache::fnt *
FGFontCache::getfnt(const char *name, float size, float slant)
{
    if (!_initialized) {
        char *envp = ::getenv("FG_FONTS");
        if (envp != NULL) {
            _path.set(envp);
        } else {
            _path.set(globals->get_fg_root());
            _path.append("Fonts");
        }

        for (int i = 0; guifonts[i].name; i++)
            _fonts[guifonts[i].name] = new fnt(guifonts[i].font);

        _initialized = true;
    }

    _itt_t it;
    if ((it = _fonts.find(name)) != _fonts.end())
        return it->second;

    SGPath path(_path);
    path.append(name);

    fnt *f = new fnt();
    f->texfont = new fntTexFont;

    if (f->texfont->load((char *)path.c_str())) {
        f->pufont = new puFont;
        f->pufont->initialize(static_cast<fntFont *>(f->texfont), size, slant);
        return _fonts[name] = f;
    }

    delete f;
    return _fonts["default"];
}

puFont *
FGFontCache::get(const char *name, float size, float slant)
{
    return getfnt(name, size, slant)->pufont;
}

fntTexFont *
FGFontCache::getTexFont(const char *name, float size, float slant)
{
    return getfnt(name, size, slant)->texfont;
}

puFont *
FGFontCache::get(SGPropertyNode *node)
{
    if (!node)
        return get("Helvetica.txf", 15.0, 0.0);

    const char *name = node->getStringValue("name", "Helvetica.txf");
    float size = node->getFloatValue("size", 15.0);
    float slant = node->getFloatValue("slant", 0.0);

    return get(name, size, slant);
}

// end of new_gui.cxx
