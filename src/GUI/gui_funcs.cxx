/**************************************************************************
 * gui_funcs.cxx
 *
 * Based on gui.cxx and renamed on 2002/08/13 by Erik Hofman.
 *
 * Written 1998 by Durk Talsma, started Juni, 1998.  For the flight gear
 * project.
 *
 * Additional mouse supported added by David Megginson, 1999.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Id$
 **************************************************************************/


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/compiler.h>

#include <fstream>
#include <string>
#include <cstring>
#include <sstream>

#include <stdlib.h>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/screen/screen-dump.hxx>

#include <Cockpit/panel.hxx>
#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Main/fg_os.hxx>
#include <Main/renderer.hxx>
#include <Main/viewmgr.hxx>
#include <GUI/new_gui.hxx>

#if defined( WIN32 ) && !defined( __CYGWIN__ ) && !defined(__MINGW32__)
#  include <simgear/screen/win32-printer.h>
#  include <simgear/screen/GlBitmaps.h>
#endif
#ifdef __MINGW32__
#include <shellapi.h>
#endif
#include "gui.h"

using std::string;


#if defined( TR_HIRES_SNAP)
#include <simgear/screen/tr.h>
extern void fgUpdateHUD( GLfloat x_start, GLfloat y_start,
                         GLfloat x_end, GLfloat y_end );
#endif


const __fg_gui_fn_t __fg_gui_fn[] = {
#ifdef TR_HIRES_SNAP
        {"dumpHiResSnapShot", fgHiResDumpWrapper},
#endif
        {"dumpSnapShot", fgDumpSnapShotWrapper},
#if defined( WIN32 ) && !defined( __CYGWIN__) && !defined(__MINGW32__)
        {"printScreen", printScreen},
#endif
        // Help
        {"helpCb", helpCb},

        // Structure termination
        {"", NULL}
};


/* ================ General Purpose Functions ================ */

// General Purpose Message Box. Makes sure no more than 5 different
// messages are displayed at the same time, and none of them are
// duplicates. (5 is a *lot*, but this will hardly ever be reached
// and we don't want to miss any, either.)
void mkDialog (const char *txt)
{
    NewGUI *gui = (NewGUI *)globals->get_subsystem("gui");
    if (!gui)
        return;
    SGPropertyNode *master = gui->getDialogProperties("message");
    if (!master)
        return;

    const int maxdialogs = 5;
    string name;
    SGPropertyNode *msg = fgGetNode("/sim/gui/dialogs", true);
    int i;
    for (i = 0; i < maxdialogs; i++) {
        std::ostringstream s;
        s << "message-" << i;
        name = s.str();

        if (!msg->getNode(name.c_str(), false))
            break;

        if (!strcmp(txt, msg->getNode(name.c_str())->getStringValue("message"))) {
            SG_LOG(SG_GENERAL, SG_WARN, "mkDialog(): duplicate of message " << txt);
            return;
        }
    }
    if (i == maxdialogs)
        return;
    msg = msg->getNode(name.c_str(), true);
    msg->setStringValue("message", txt);
    msg = msg->getNode("dialog", true);
    copyProperties(master, msg);
    msg->setStringValue("name", name.c_str());
    gui->newDialog(msg);
    gui->showDialog(name.c_str());
}

// Message Box to report an error.
void guiErrorMessage (const char *txt)
{
    SG_LOG(SG_GENERAL, SG_ALERT, txt);
    mkDialog(txt);
}

// Message Box to report a throwable (usually an exception).
void guiErrorMessage (const char *txt, const sg_throwable &throwable)
{
    string msg = txt;
    msg += '\n';
    msg += throwable.getFormattedMessage();
    if (!std::strlen(throwable.getOrigin()) != 0) {
        msg += "\n (reported by ";
        msg += throwable.getOrigin();
        msg += ')';
    }
    SG_LOG(SG_GENERAL, SG_ALERT, msg);
    mkDialog(msg.c_str());
}



/* -----------------------------------------------------------------------
the Gui callback functions 
____________________________________________________________________*/


// Hier Neu :-) This is my newly added code
// Added by David Findlay <nedz@bigpond.com>
// on Sunday 3rd of December


void helpCb ()
{
    string command;
	
    SGPath path( globals->get_fg_root() );
    path.append( "Docs/index.html" );
	
#if !defined(WIN32)

    command = globals->get_browser();
    string::size_type pos;
    if ((pos = command.find("%u", 0)) != string::npos)
        command.replace(pos, 2, path.str());
    else
        command += " " + path.str();

    command += " &";
    system( command.c_str() );

#else // WIN32

    // Look for favorite browser
    char Dummy[1024], ExecName[1024], browserParameter[1024];
    char win32_name[1024];
# ifdef __CYGWIN__
    cygwin32_conv_to_full_win32_path(path.c_str(),win32_name);
# else
    strncpy(win32_name,path.c_str(), 1024);
# endif
    Dummy[0] = 0;
    FindExecutable(win32_name, Dummy, ExecName);
    snprintf(browserParameter, 1024, "file:///%s", win32_name);
    ShellExecute ( NULL, "open", ExecName, browserParameter, Dummy,
                   SW_SHOWNORMAL ) ;

#endif
	
    mkDialog ("Help started in your web browser window.");
}

#if defined( TR_HIRES_SNAP)
void fgHiResDump()
{
    FILE *f;
    string message;
    bool menu_status = fgGetBool("/sim/menubar/visibility");
    char *filename = new char [24];
    static int count = 1;

    static const SGPropertyNode *master_freeze
        = fgGetNode("/sim/freeze/master");

    bool freeze = master_freeze->getBoolValue();
    if ( !freeze ) {
        fgSetBool("/sim/freeze/master", true);
    }

    fgSetBool("/sim/menubar/visibility", false);
    int mouse = fgGetMouseCursor();
    fgSetMouseCursor(MOUSE_CURSOR_NONE);

    FGRenderer *renderer = globals->get_renderer();
//     renderer->init();
    renderer->resize( fgGetInt("/sim/startup/xsize"),
                      fgGetInt("/sim/startup/ysize") );

    // we need two render frames here to clear the menu and cursor
    // ... not sure why but doing an extra fgRenderFrame() shouldn't
    // hurt anything
    //renderer->update( true );
    //renderer->update( true );

    // This ImageSize stuff is a temporary hack
    // should probably use 128x128 tile size and
    // support any image size

    // This should be a requester to get multiplier from user
    int multiplier = fgGetInt("/sim/startup/hires-multiplier", 3);
    int width = fgGetInt("/sim/startup/xsize");
    int height = fgGetInt("/sim/startup/ysize");
	
    /* allocate buffer large enough to store one tile */
    GLubyte *tile = (GLubyte *)malloc(width * height * 3 * sizeof(GLubyte));
    if (!tile) {
        delete [] filename;
        printf("Malloc of tile buffer failed!\n");
        return;
    }

    int imageWidth  = multiplier*width;
    int imageHeight = multiplier*height;

    /* allocate buffer to hold a row of tiles */
    GLubyte *buffer
        = (GLubyte *)malloc(imageWidth * height * 3 * sizeof(GLubyte));
    if (!buffer) {
        delete [] filename;
        free(tile);
        printf("Malloc of tile row buffer failed!\n");
        return;
    }
    TRcontext *tr = trNew();
    trTileSize(tr, width, height, 0);
    trTileBuffer(tr, GL_RGB, GL_UNSIGNED_BYTE, tile);
    trImageSize(tr, imageWidth, imageHeight);
    trRowOrder(tr, TR_TOP_TO_BOTTOM);
    // OSGFIXME
//     sgFrustum *frustum = ssgGetFrustum();
//     trFrustum(tr,
//               frustum->getLeft(), frustum->getRight(),
//               frustum->getBot(),  frustum->getTop(), 
//               frustum->getNear(), frustum->getFar());
	
    /* Prepare ppm output file */
    while (count < 1000) {
        snprintf(filename, 24, "fgfs-screen-%03d.ppm", count++);
        if ( (f = fopen(filename, "r")) == NULL )
            break;
        fclose(f);
    }
    f = fopen(filename, "wb");
    if (!f) {
        printf("Couldn't open image file: %s\n", filename);
        delete [] filename;
        free(buffer);
        free(tile);
        return;
    }
    fprintf(f,"P6\n");
    fprintf(f,"# ppm-file created by %s\n", "trdemo2");
    fprintf(f,"%i %i\n", imageWidth, imageHeight);
    fprintf(f,"255\n");

    /* just to be safe... */
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // OSGFIXME
#if 0
    /* Because the HUD and Panel change the ViewPort we will
     * need to handle some lowlevel stuff ourselves */
    int ncols = trGet(tr, TR_COLUMNS);
    int nrows = trGet(tr, TR_ROWS);

    bool do_hud = fgGetBool("/sim/hud/visibility");
    GLfloat hud_col_step = 640.0 / ncols;
    GLfloat hud_row_step = 480.0 / nrows;
	
    bool do_panel = fgPanelVisible();
    GLfloat panel_col_step = globals->get_current_panel()->getWidth() / ncols;
    GLfloat panel_row_step = globals->get_current_panel()->getHeight() / nrows;
#endif
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glHint(GL_FOG_HINT, GL_NICEST);
	
    /* Draw tiles */
    int more = 1;
    while (more) {
        trBeginTile(tr);
        int curColumn = trGet(tr, TR_CURRENT_COLUMN);
        // int curRow =  trGet(tr, TR_CURRENT_ROW);

        renderer->update( false );
        // OSGFIXME
//         if ( do_hud )
//             fgUpdateHUD( curColumn*hud_col_step,      curRow*hud_row_step,
//                          (curColumn+1)*hud_col_step, (curRow+1)*hud_row_step );
        // OSGFIXME
//         if (do_panel)
//             globals->get_current_panel()->update(
//                                    curColumn*panel_col_step, panel_col_step,
//                                    curRow*panel_row_step,    panel_row_step );
        more = trEndTile(tr);

        /* save tile into tile row buffer*/
        int curTileWidth = trGet(tr, TR_CURRENT_TILE_WIDTH);
        int bytesPerImageRow = imageWidth * 3*sizeof(GLubyte);
        int bytesPerTileRow = (width) * 3*sizeof(GLubyte);
        int xOffset = curColumn * bytesPerTileRow;
        int bytesPerCurrentTileRow = (curTileWidth) * 3*sizeof(GLubyte);
        int i;
        for (i=0;i<height;i++) {
            memcpy(buffer + i*bytesPerImageRow + xOffset, /* Dest */
                   tile + i*bytesPerTileRow,              /* Src */
                   bytesPerCurrentTileRow);               /* Byte count*/
        }

        if (curColumn == trGet(tr, TR_COLUMNS)-1) {
            /* write this buffered row of tiles to the file */
            int curTileHeight = trGet(tr, TR_CURRENT_TILE_HEIGHT);
            int bytesPerImageRow = imageWidth * 3*sizeof(GLubyte);
            int i;
            for (i=0;i<curTileHeight;i++) {
                /* Remember, OpenGL images are bottom to top.  Have to reverse. */
                GLubyte *rowPtr = buffer + (curTileHeight-1-i) * bytesPerImageRow;
                fwrite(rowPtr, 1, imageWidth*3, f);
            }
        }

    }

    renderer->resize( width, height );

    trDelete(tr);

    glHint(GL_POLYGON_SMOOTH_HINT, GL_DONT_CARE);
    glHint(GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
    glHint(GL_POINT_SMOOTH_HINT, GL_DONT_CARE);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);
    if ( (!strcmp(fgGetString("/sim/rendering/fog"), "disabled")) ||
         (!fgGetBool("/sim/rendering/shading"))) {
        // if fastest fog requested, or if flat shading force fastest
        glHint ( GL_FOG_HINT, GL_FASTEST );
    } else if ( !strcmp(fgGetString("/sim/rendering/fog"), "nicest") ) {
        glHint ( GL_FOG_HINT, GL_DONT_CARE );
    }

    fclose(f);

    message = "Snapshot saved to \"";
    message += filename;
    message += "\".";
    mkDialog (message.c_str());

    free(tile);
    free(buffer);

    delete [] filename;

    fgSetMouseCursor(mouse);
    fgSetBool("/sim/menubar/visibility", menu_status);

    if ( !freeze ) {
        fgSetBool("/sim/freeze/master", false);
    }
}
#endif // #if defined( TR_HIRES_SNAP)


#if defined( WIN32 ) && !defined( __CYGWIN__) && !defined(__MINGW32__)

void rotateView( double roll, double pitch, double yaw )
{
	// rotate view
}

GlBitmap *b1 = NULL;
GLubyte *hiResScreenCapture( int multiplier )
{
    float oldfov = fgGetDouble("/sim/current-view/field-of-view");
    float fov = oldfov / multiplier;
    FGViewer *v = globals->get_current_view();
    fgSetDouble("/sim/current-view/field-of-view", fov);
//     globals->get_renderer()->init();
    int cur_width = fgGetInt("/sim/startup/xsize");
    int cur_height = fgGetInt("/sim/startup/ysize");
    delete( b1 );
    // New empty (mostly) bitmap
    b1 = new GlBitmap( GL_RGB, 1, 1, (unsigned char *)"123" );
    int x,y;
    for ( y = 0; y < multiplier; y++ ) {
	for ( x = 0; x < multiplier; x++ ) {
	    globals->get_renderer()->resize( cur_width, cur_height );
	    // pan to tile
	    rotateView( 0, (y*fov)-((multiplier-1)*fov/2), (x*fov)-((multiplier-1)*fov/2) );
	    globals->get_renderer()->update( false );
	    // restore view
	    GlBitmap b2;
	    b1->copyBitmap( &b2, cur_width*x, cur_height*y );
	}
    }
    fgSetDouble("/sim/current-view/field-of-view", oldfov);
    return b1->getBitmap();
}
#endif

#if defined( WIN32 ) && !defined( __CYGWIN__) && !defined(__MINGW32__)
// win32 print screen function
void printScreen () {
    int mouse = fgGetMouseCursor();
    fgSetMouseCursor(MOUSE_CURSOR_NONE);

    CGlPrinter p( CGlPrinter::PRINT_BITMAP );
    int cur_width = fgGetInt("/sim/startup/xsize");
    int cur_height = fgGetInt("/sim/startup/ysize");
    p.Begin( "FlightGear", cur_width*3, cur_height*3 );
    p.End( hiResScreenCapture(3) );

    fgSetMouseCursor(mouse);
}
#endif // #ifdef WIN32


void fgDumpSnapShotWrapper () {
    fgDumpSnapShot();
}


void fgHiResDumpWrapper () {
    fgHiResDump();
}


// do a screen snap shot
bool fgDumpSnapShot () {
    static SGConstPropertyNode_ptr master_freeze = fgGetNode("/sim/freeze/master");

    bool freeze = master_freeze->getBoolValue();
    if ( !freeze ) {
        fgSetBool("/sim/freeze/master", true);
    }

    int mouse = fgGetMouseCursor();
    fgSetMouseCursor(MOUSE_CURSOR_NONE);

    fgSetBool("/sim/signals/screenshot", true);

    FGRenderer *renderer = globals->get_renderer();
    renderer->resize( fgGetInt("/sim/startup/xsize"),
                      fgGetInt("/sim/startup/ysize") );

    // we need two render frames here to clear the menu and cursor
    // ... not sure why but doing an extra fgRenderFrame() shouldn't
    // hurt anything
    renderer->update( true );
    renderer->update( true );

    string dir = fgGetString("/sim/paths/screenshot-dir");
    if (dir.empty())
        dir = fgGetString("/sim/fg-current");

    SGPath path(dir + '/');
    if (path.create_dir( 0755 )) {
        SG_LOG(SG_GENERAL, SG_ALERT, "Cannot create screenshot directory '"
                << dir << "'. Trying home directory.");
        dir = fgGetString("/sim/fg-home");
    }

    char filename[24];
    static int count = 1;
    while (count < 1000) {
        snprintf(filename, 24, "fgfs-screen-%03d.png", count++);

        SGPath p(dir);
        p.append(filename);
        if (!p.exists()) {
            path.set(p.str());
            break;
        }
    }

    int result = sg_glDumpWindow(path.c_str(),
                                 fgGetInt("/sim/startup/xsize"),
                                 fgGetInt("/sim/startup/ysize"));

    fgSetString("/sim/paths/screenshot-last", path.c_str());
    fgSetBool("/sim/signals/screenshot", false);

    fgSetMouseCursor(mouse);

    if ( !freeze ) {
        fgSetBool("/sim/freeze/master", false);
    }
    return result != 0;
}

// do an entire scenegraph dump
void fgDumpSceneGraph()
{
    char *filename = new char [24];
    string message;
    static int count = 1;

    static const SGPropertyNode *master_freeze
	= fgGetNode("/sim/freeze/master");

    bool freeze = master_freeze->getBoolValue();
    if ( !freeze ) {
        fgSetBool("/sim/freeze/master", true);
    }

    while (count < 1000) {
        FILE *fp;
        snprintf(filename, 24, "fgfs-graph-%03d.osg", count++);
        if ( (fp = fopen(filename, "r")) == NULL )
            break;
        fclose(fp);
    }

    if ( fgDumpSceneGraphToFile(filename)) {
	message = "Entire scene graph saved to \"";
	message += filename;
	message += "\".";
    } else {
        message = "Failed to save to \"";
	message += filename;
	message += "\".";
    }

    mkDialog (message.c_str());

    delete [] filename;

    if ( !freeze ) {
        fgSetBool("/sim/freeze/master", false);
    }
}

    
// do an terrain branch dump
void fgDumpTerrainBranch()
{
    char *filename = new char [24];
    string message;
    static int count = 1;

    static const SGPropertyNode *master_freeze
	= fgGetNode("/sim/freeze/master");

    bool freeze = master_freeze->getBoolValue();
    if ( !freeze ) {
        fgSetBool("/sim/freeze/master", true);
    }

    while (count < 1000) {
        FILE *fp;
        snprintf(filename, 24, "fgfs-graph-%03d.osg", count++);
        if ( (fp = fopen(filename, "r")) == NULL )
            break;
        fclose(fp);
    }

    if ( fgDumpTerrainBranchToFile(filename)) {
	message = "Terrain graph saved to \"";
	message += filename;
	message += "\".";
    } else {
        message = "Failed to save to \"";
	message += filename;
	message += "\".";
    }

    mkDialog (message.c_str());

    delete [] filename;

    if ( !freeze ) {
        fgSetBool("/sim/freeze/master", false);
    }
}

    

