



extern "C" {

#include <assert.h>
#include <float.h>
#include <syslog.h>

#include "vim.h"
#include "version.h"

}   




#include <Application.h>
#include <Beep.h>
#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <Clipboard.h>
#include <Debug.h>


#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>

#include <IconUtils.h>
#include <Input.h>
#include <ListView.h>
#include <MenuBar.h>
#include <MenuItem.h>


#include <Path.h>
#include <PictureButton.h>
#include <PopUpMenu.h>

#include <Resources.h>

#include <Screen.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>

#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <View.h>
#include <Window.h>

class VimApp;
class VimFormView;
class VimTextAreaView;
class VimWindow;
class VimToolbar;
class VimTabLine;

extern key_map *keyMap;
extern char *keyMapChars;

extern int main(int argc, char **argv);

#ifndef B_MAX_PORT_COUNT
#define B_MAX_PORT_COUNT    255
#endif


class VimApp: public BApplication
{
    typedef BApplication Inherited;
    public:
    VimApp(const char *appsig);
    ~VimApp();

    
#if 0
    virtual void DispatchMessage(BMessage *m, BHandler *h)
    {
	m->PrintToStream();
	Inherited::DispatchMessage(m, h);
    }
#endif
    virtual void ReadyToRun();
    virtual void ArgvReceived(int32 argc, char **argv);
    virtual void RefsReceived(BMessage *m);
    virtual bool QuitRequested();
    virtual void MessageReceived(BMessage *m);

    static void SendRefs(BMessage *m, bool changedir);

    sem_id	fFilePanelSem;
    BFilePanel*	fFilePanel;
    BPath	fBrowsedPath;
    private:
};

class VimWindow: public BWindow
{
    typedef BWindow Inherited;
    public:
    VimWindow();
    ~VimWindow();

    
    virtual void WindowActivated(bool active);
    virtual bool QuitRequested();

    VimFormView	    *formView;

    private:
    void init();

};

class VimFormView: public BView
{
    typedef BView Inherited;
    public:
    VimFormView(BRect frame);
    ~VimFormView();

    
    virtual void AllAttached();
    virtual void FrameResized(float new_width, float new_height);

#define MENUBAR_MARGIN	1
    float MenuHeight() const
    { return menuBar ? menuBar->Frame().Height() + MENUBAR_MARGIN: 0; }
    BMenuBar *MenuBar() const
    { return menuBar; }

    private:
    void init(BRect);

    BMenuBar	    *menuBar;
    VimTextAreaView *textArea;

#ifdef FEAT_TOOLBAR
    public:
    float ToolbarHeight() const;
    VimToolbar *ToolBar() const
    { return toolBar; }
    private:
    VimToolbar	    *toolBar;
#endif

#ifdef FEAT_GUI_TABLINE
    public:
    VimTabLine *TabLine() const	{ return tabLine; }
    bool IsShowingTabLine() const { return showingTabLine; }
    void SetShowingTabLine(bool showing) { showingTabLine = showing;	}
    float TablineHeight() const;
    private:
    VimTabLine	*tabLine;
    int	showingTabLine;
#endif
};

class VimTextAreaView: public BView
{
    typedef BView Inherited;
    public:
    VimTextAreaView(BRect frame);
    ~VimTextAreaView();

    
    virtual void Draw(BRect updateRect);
    virtual void KeyDown(const char *bytes, int32 numBytes);
    virtual void MouseDown(BPoint point);
    virtual void MouseUp(BPoint point);
    virtual void MouseMoved(BPoint point, uint32 transit, const BMessage *message);
    virtual void MessageReceived(BMessage *m);

    
    int mchInitFont(char_u *name);
    void mchDrawString(int row, int col, char_u *s, int len, int flags);
    void mchClearBlock(int row1, int col1, int row2, int col2);
    void mchClearAll();
    void mchDeleteLines(int row, int num_lines);
    void mchInsertLines(int row, int num_lines);

    static void guiSendMouseEvent(int button, int x, int y, int repeated_click, int_u modifiers);
    static void guiMouseMoved(int x, int y);
    static void guiBlankMouse(bool should_hide);
    static int_u mouseModifiersToVim(int32 beModifiers);

    int32 mouseDragEventCount;

#ifdef FEAT_MBYTE_IME
    void DrawIMString(void);
#endif

    private:
    void init(BRect);

    int_u	vimMouseButton;
    int_u	vimMouseModifiers;

#ifdef FEAT_MBYTE_IME
    struct {
	BMessenger* messenger;
	BMessage* message;
	BPoint location;
	int row;
	int col;
	int count;
    } IMData;
#endif
};

class VimScrollBar: public BScrollBar
{
    typedef BScrollBar Inherited;
    public:
    VimScrollBar(scrollbar_T *gsb, orientation posture);
    ~VimScrollBar();

    virtual void ValueChanged(float newValue);
    virtual void MouseUp(BPoint where);
    void SetValue(float newval);
    scrollbar_T *getGsb()
    { return gsb; }

    int32	scrollEventCount;

    private:
    scrollbar_T *gsb;
    float   ignoreValue;
};


#ifdef FEAT_TOOLBAR

class VimToolbar : public BBox
{
    static BBitmap *normalButtonsBitmap;
    static BBitmap *grayedButtonsBitmap;

    BBitmap *LoadVimBitmap(const char* fileName);
    bool GetPictureFromBitmap(BPicture *pictureTo, int32 index, BBitmap *bitmapFrom, bool pressed);
    bool ModifyBitmapToGrayed(BBitmap *bitmap);

    BList fButtonsList;
    void InvalidateLayout();

    public:
    VimToolbar(BRect frame, const char * name);
    ~VimToolbar();

    bool PrepareButtonBitmaps();

    bool AddButton(int32 index, vimmenu_T *menu);
    bool RemoveButton(vimmenu_T *menu);
    bool GrayButton(vimmenu_T *menu, int grey);

    float ToolbarHeight() const;
    virtual void AttachedToWindow();
};

BBitmap *VimToolbar::normalButtonsBitmap  = NULL;
BBitmap *VimToolbar::grayedButtonsBitmap  = NULL;

const float ToolbarMargin = 3.;
const float ButtonMargin  = 3.;

#endif 

#ifdef FEAT_GUI_TABLINE

class VimTabLine : public BTabView
{
    public:
	class VimTab : public BTab {
	    public:
		VimTab() : BTab(new BView(BRect(), "-Empty-", 0, 0)) {}

	    virtual void Select(BView* owner);
	};

	VimTabLine(BRect r) : BTabView(r, "vimTabLine", B_WIDTH_FROM_LABEL,
	       B_FOLLOW_LEFT | B_FOLLOW_TOP | B_FOLLOW_RIGHT, B_WILL_DRAW | B_FRAME_EVENTS) {}

    float TablineHeight() const;
    virtual void MouseDown(BPoint point);
};

#endif 




class VimFont: public BFont
{
    typedef BFont Inherited;
    public:
    VimFont();
    VimFont(const VimFont *rhs);
    VimFont(const BFont *rhs);
    VimFont(const VimFont &rhs);
    ~VimFont();

    VimFont *next;
    int refcount;
    char_u *name;

    private:
    void init();
};

#if defined(FEAT_GUI_DIALOG)

class VimDialog : public BWindow
{
    typedef BWindow Inherited;

    BButton* _CreateButton(int32 which, const char* label);

    public:

    class View : public BView {
	typedef BView Inherited;

	public:
	View(BRect frame);
	~View();

	virtual void Draw(BRect updateRect);
	void InitIcon(int32 type);

	private:
	BBitmap*    fIconBitmap;
    };

    VimDialog(int type, const char *title, const char *message,
	    const char *buttons, int dfltbutton, const char *textfield,
	    int ex_cmd);
    ~VimDialog();

    int Go();

    virtual void MessageReceived(BMessage *msg);

    private:
    sem_id	    fDialogSem;
    int		    fDialogValue;
    BList	    fButtonsList;
    BTextView*	    fMessageView;
    BTextControl*   fInputControl;
    const char*	    fInputValue;
};

class VimSelectFontDialog : public BWindow
{
    typedef BWindow Inherited;

    void _CleanList(BListView* list);
    void _UpdateFontStyles();
    void _UpdateSizeInputPreview();
    void _UpdateFontPreview();
    bool _UpdateFromListItem(BListView* list, char* text, int textSize);
    public:

    VimSelectFontDialog(font_family* family, font_style* style, float* size);
    ~VimSelectFontDialog();

    bool Go();

    virtual void MessageReceived(BMessage *msg);

    private:
    status_t	    fStatus;
    sem_id	    fDialogSem;
    bool	    fDialogValue;
    font_family*    fFamily;
    font_style*	    fStyle;
    float*	    fSize;
    font_family	    fFontFamily;
    font_style	    fFontStyle;
    float	    fFontSize;
    BStringView*    fPreview;
    BListView*	    fFamiliesList;
    BListView*	    fStylesList;
    BListView*	    fSizesList;
    BTextControl*   fSizesInput;
};

#endif 



struct MainArgs {
    int	     argc;
    char    **argv;
};






#define	KEY_MSG_BUFSIZ	7
#if KEY_MSG_BUFSIZ < MAX_KEY_CODE_LEN
#error Increase KEY_MSG_BUFSIZ!
#endif

struct VimKeyMsg {
    char_u  length;
    char_u  chars[KEY_MSG_BUFSIZ];  
    bool    csi_escape;
};

struct VimResizeMsg {
    int	    width;
    int	    height;
};

struct VimScrollBarMsg {
    VimScrollBar *sb;
    long    value;
    int	    stillDragging;
};

struct VimMenuMsg {
    vimmenu_T	*guiMenu;
};

struct VimMouseMsg {
    int	    button;
    int	    x;
    int	    y;
    int	    repeated_click;
    int_u   modifiers;
};

struct VimMouseMovedMsg {
    int	    x;
    int	    y;
};

struct VimFocusMsg {
    bool    active;
};

struct VimRefsMsg {
    BMessage   *message;
    bool    changedir;
};

struct VimTablineMsg {
    int	    index;
};

struct VimTablineMenuMsg {
    int	    index;
    int	    event;
};

struct VimMsg {
    enum VimMsgType {
	Key, Resize, ScrollBar, Menu, Mouse, MouseMoved, Focus, Refs, Tabline, TablineMenu
    };

    union {
	struct VimKeyMsg    Key;
	struct VimResizeMsg NewSize;
	struct VimScrollBarMsg	Scroll;
	struct VimMenuMsg   Menu;
	struct VimMouseMsg  Mouse;
	struct VimMouseMovedMsg	MouseMoved;
	struct VimFocusMsg  Focus;
	struct VimRefsMsg   Refs;
	struct VimTablineMsg	Tabline;
	struct VimTablineMenuMsg    TablineMenu;
    } u;
};

#define RGB(r, g, b)	((char_u)(r) << 16 | (char_u)(g) << 8 | (char_u)(b) << 0)
#define GUI_TO_RGB(g)	{ (char_u)((g) >> 16), (char_u)((g) >> 8), (char_u)((g) >> 0), 255 }



static struct specialkey
{
    uint16  BeKeys;
#define KEY(a,b)    ((a)<<8|(b))
#define K(a)	    KEY(0,a)		
#define F(b)	    KEY(1,b)		
    char_u  vim_code0;
    char_u  vim_code1;
} special_keys[] =
{
    {K(B_UP_ARROW),	'k', 'u'},
    {K(B_DOWN_ARROW),	    'k', 'd'},
    {K(B_LEFT_ARROW),	    'k', 'l'},
    {K(B_RIGHT_ARROW),	    'k', 'r'},
    {K(B_BACKSPACE),	    'k', 'b'},
    {K(B_INSERT),	'k', 'I'},
    {K(B_DELETE),	'k', 'D'},
    {K(B_HOME),		'k', 'h'},
    {K(B_END),		'@', '7'},
    {K(B_PAGE_UP),	'k', 'P'},	
    {K(B_PAGE_DOWN),	    'k', 'N'},	    

#define FIRST_FUNCTION_KEY  11
    {F(B_F1_KEY),	'k', '1'},
    {F(B_F2_KEY),	'k', '2'},
    {F(B_F3_KEY),	'k', '3'},
    {F(B_F4_KEY),	'k', '4'},
    {F(B_F5_KEY),	'k', '5'},
    {F(B_F6_KEY),	'k', '6'},
    {F(B_F7_KEY),	'k', '7'},
    {F(B_F8_KEY),	'k', '8'},
    {F(B_F9_KEY),	'k', '9'},
    {F(B_F10_KEY),	'k', ';'},

    {F(B_F11_KEY),	'F', '1'},
    {F(B_F12_KEY),	'F', '2'},
    
    
    {F(0x0F),		'F', '4'},	
    {F(0x10),		'F', '5'},	
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    

    
    {F(B_PRINT_KEY),	    '%', '9'},

#if 0
    
    {F(0x48),	    'k', 'l'},	    
    {F(0x4A),	    'k', 'r'},	    
    {F(0x38),	    'k', 'u'},	    
    {F(0x59),	    'k', 'd'},	    
    {F(0x64),	    'k', 'I'},	    
    {F(0x65),	    'k', 'D'},	    
    {F(0x37),	    'k', 'h'},	    
    {F(0x58),	    '@', '7'},	    
    {F(0x39),	    'k', 'P'},	    
    {F(0x60),	    'k', 'N'},	    
    {F(0x49),	    '&', '8'},	    
#endif

    
    {0,		    0, 0}
};

#define NUM_SPECIAL_KEYS    ARRAY_LENGTH(special_keys)



    static void
docd(BPath &path)
{
    mch_chdir((char *)path.Path());
    
    do_cmdline_cmd((char_u *)"cd .");
}

	static void
drop_callback(void *cookie)
{
    
    
    
    update_screen(UPD_NOT_VALID);
}

    
	static void
RefsReceived(BMessage *m, bool changedir)
{
    uint32 type;
    int32 count;

    m->PrintToStream();
    switch (m->what) {
	case B_REFS_RECEIVED:
	case B_SIMPLE_DATA:
	    m->GetInfo("refs", &type, &count);
	    if (type != B_REF_TYPE)
		goto bad;
	    break;
	case B_ARGV_RECEIVED:
	    m->GetInfo("argv", &type, &count);
	    if (type != B_STRING_TYPE)
		goto bad;
	    if (changedir) {
		char *dirname;
		if (m->FindString("cwd", (const char **) &dirname) == B_OK) {
		    chdir(dirname);
		    do_cmdline_cmd((char_u *)"cd .");
		}
	    }
	    break;
	default:
bad:
	    
	    delete m;
	    return;
    }

#ifdef FEAT_VISUAL
    reset_VIsual();
#endif

    char_u  **fnames;
    fnames = (char_u **) alloc(count * sizeof(char_u *));
    int fname_index = 0;

    switch (m->what) {
	case B_REFS_RECEIVED:
	case B_SIMPLE_DATA:
	    
	    for (int i = 0; i < count; ++i)
	    {
		entry_ref ref;
		if (m->FindRef("refs", i, &ref) == B_OK) {
		    BEntry entry(&ref, false);
		    BPath path;
		    entry.GetPath(&path);

		    
		    if (changedir) {
			BPath parentpath;
			path.GetParent(&parentpath);
			docd(parentpath);
		    }

		    
		    BDirectory bdir(&ref);
		    if (bdir.InitCheck() == B_OK) {
			
			if (!changedir)
			    docd(path);
		    } else {
			mch_dirname(IObuff, IOSIZE);
			char_u *fname = shorten_fname((char_u *)path.Path(), IObuff);
			if (fname == NULL)
			    fname = (char_u *)path.Path();
			fnames[fname_index++] = vim_strsave(fname);
			
		    }

		    
		    changedir = false;
		}
	    }
	    break;
	case B_ARGV_RECEIVED:
	    
	    for (int i = 1; i < count; ++i)
	    {
		char *fname;

		if (m->FindString("argv", i, (const char **) &fname) == B_OK) {
		    fnames[fname_index++] = vim_strsave((char_u *)fname);
		}
	    }
	    break;
	default:
	    
	    break;
    }

    delete m;

    
    if (fname_index > 0) {
	handle_drop(fname_index, fnames, FALSE, drop_callback, NULL);

	setcursor();
	out_flush();
    } else {
	vim_free(fnames);
    }
}

VimApp::VimApp(const char *appsig):
    BApplication(appsig),
    fFilePanelSem(-1),
    fFilePanel(NULL)
{
}

VimApp::~VimApp()
{
}

    void
VimApp::ReadyToRun()
{
    
    mch_signal(SIGINT, SIG_IGN);
    mch_signal(SIGQUIT, SIG_IGN);
}

    void
VimApp::ArgvReceived(int32 arg_argc, char **arg_argv)
{
    if (!IsLaunching()) {
	
	if (gui.vimWindow)
	    gui.vimWindow->Minimize(false);
	BMessage *m = CurrentMessage();
	DetachCurrentMessage();
	SendRefs(m, true);
    }
}

    void
VimApp::RefsReceived(BMessage *m)
{
    
    
    
    
    int limit = 15;
    while (--limit >= 0 && (curbuf == NULL || curbuf->b_start_ffc == 0))
	snooze(100000);    
    if (gui.vimWindow)
	gui.vimWindow->Minimize(false);
    DetachCurrentMessage();
    SendRefs(m, true);
}


    void
VimApp::SendRefs(BMessage *m, bool changedir)
{
    VimRefsMsg rm;
    rm.message = m;
    rm.changedir = changedir;

    write_port(gui.vdcmp, VimMsg::Refs, &rm, sizeof(rm));
    
}

    void
VimApp::MessageReceived(BMessage *m)
{
    switch (m->what) {
	case 'save':
	    {
		entry_ref refDirectory;
		m->FindRef("directory", &refDirectory);
		fBrowsedPath.SetTo(&refDirectory);
		BString strName;
		m->FindString("name", &strName);
		fBrowsedPath.Append(strName.String());
	    }
	    break;
	case 'open':
	    {
		entry_ref ref;
		m->FindRef("refs", &ref);
		fBrowsedPath.SetTo(&ref);
	    }
	    break;
	case B_CANCEL:
	    {
		BFilePanel *panel;
		m->FindPointer("source", (void**)&panel);
		if (fFilePanelSem != -1 && panel == fFilePanel)
		{
		    delete_sem(fFilePanelSem);
		    fFilePanelSem = -1;
		}

	    }
	    break;
	default:
	    Inherited::MessageReceived(m);
	    break;
    }
}

    bool
VimApp::QuitRequested()
{
    (void)Inherited::QuitRequested();
    return false;
}



VimWindow::VimWindow():
    BWindow(BRect(40, 40, 150, 150),
	    "Vim",
	    B_TITLED_WINDOW,
	    0,
	    B_CURRENT_WORKSPACE)

{
    init();
}

VimWindow::~VimWindow()
{
    if (formView) {
	RemoveChild(formView);
	delete formView;
    }
    gui.vimWindow = NULL;
}

    void
VimWindow::init()
{
    
    formView = new VimFormView(Bounds());
    if (formView != NULL) {
	AddChild(formView);
    }
}

#if 0  
    void
VimWindow::DispatchMessage(BMessage *m, BHandler *h)
{
    
    switch (m->what) {
	case B_MOUSE_UP:
	    
	    
	    
	    
	    {
		BView *v = dynamic_cast<BView *>(h);
		if (v) {
		    
		    BPoint where;
		    m->FindPoint("where", &where);
		    v->MouseUp(where);
		} else {
		    Inherited::DispatchMessage(m, h);
		}
	    }
	    break;
	default:
	    Inherited::DispatchMessage(m, h);
    }
}
#endif

    void
VimWindow::WindowActivated(bool active)
{
    Inherited::WindowActivated(active);
    
    if (active && gui.vimTextArea)
	gui.vimTextArea->MakeFocus(true);

    struct VimFocusMsg fm;
    fm.active = active;

    write_port(gui.vdcmp, VimMsg::Focus, &fm, sizeof(fm));
}

    bool
VimWindow::QuitRequested()
{
    struct VimKeyMsg km;
    km.length = 5;
    memcpy((char *)km.chars, "\033:qa\r", km.length);
    km.csi_escape = false;
    write_port(gui.vdcmp, VimMsg::Key, &km, sizeof(km));
    return false;
}



VimFormView::VimFormView(BRect frame):
    BView(frame, "VimFormView", B_FOLLOW_ALL_SIDES,
	    B_WILL_DRAW | B_FRAME_EVENTS),
    menuBar(NULL),
#ifdef FEAT_TOOLBAR
    toolBar(NULL),
#endif
#ifdef FEAT_GUI_TABLINE

    tabLine(NULL),
#endif
    textArea(NULL)
{
    init(frame);
}

VimFormView::~VimFormView()
{
    if (menuBar) {
	RemoveChild(menuBar);
#ifdef never
	
	
	delete menuBar;
#endif
    }

#ifdef FEAT_TOOLBAR
    delete toolBar;
#endif

#ifdef FEAT_GUI_TABLINE
    delete tabLine;
#endif

    if (textArea) {
	RemoveChild(textArea);
	delete textArea;
    }
    gui.vimForm = NULL;
}

    void
VimFormView::init(BRect frame)
{
    menuBar = new BMenuBar(BRect(0,0,-MENUBAR_MARGIN,-MENUBAR_MARGIN),
	    "VimMenuBar");

    AddChild(menuBar);

#ifdef FEAT_TOOLBAR
    toolBar = new VimToolbar(BRect(0,0,0,0), "VimToolBar");
    toolBar->PrepareButtonBitmaps();
    AddChild(toolBar);
#endif

#ifdef FEAT_GUI_TABLINE
    tabLine = new VimTabLine(BRect(0,0,0,0));

    AddChild(tabLine);
#endif

    BRect remaining = frame;
    textArea = new VimTextAreaView(remaining);
    AddChild(textArea);
    

    gui.vimForm = this;
}

#ifdef FEAT_TOOLBAR
    float
VimFormView::ToolbarHeight() const
{
    return toolBar ? toolBar->ToolbarHeight() : 0.;
}
#endif

#ifdef FEAT_GUI_TABLINE
    float
VimFormView::TablineHeight() const
{
    return (tabLine && IsShowingTabLine()) ? tabLine->TablineHeight() : 0.;
}
#endif

    void
VimFormView::AllAttached()
{
    
    mch_signal(SIGINT, SIG_IGN);
    mch_signal(SIGQUIT, SIG_IGN);

    if (menuBar && textArea) {
	
	BRect remaining = Bounds();

#ifdef FEAT_MENU
	remaining.top += MenuHeight();
	menuBar->ResizeTo(remaining.right, remaining.top);
	gui.menu_height = (int) MenuHeight();
#endif

#ifdef FEAT_TOOLBAR
	toolBar->MoveTo(remaining.left, remaining.top);
	toolBar->ResizeTo(remaining.right, ToolbarHeight());
	remaining.top += ToolbarHeight();
	gui.toolbar_height = ToolbarHeight();
#endif

#ifdef FEAT_GUI_TABLINE
	tabLine->MoveTo(remaining.left, remaining.top);
	tabLine->ResizeTo(remaining.right + 1, TablineHeight());
	remaining.top += TablineHeight();
	gui.tabline_height = TablineHeight();
#endif

	textArea->ResizeTo(remaining.Width(), remaining.Height());
	textArea->MoveTo(remaining.left, remaining.top);
    }


    Inherited::AllAttached();
}

    void
VimFormView::FrameResized(float new_width, float new_height)
{
    struct VimResizeMsg sm;
    int adjust_h, adjust_w;

    new_width += 1;	
    new_height += 1;

    sm.width = (int) new_width;
    sm.height = (int) new_height;
    adjust_w = ((int)new_width - gui_get_base_width()) % gui.char_width;
    adjust_h = ((int)new_height - gui_get_base_height()) % gui.char_height;

    if (adjust_w > 0 || adjust_h > 0) {
	sm.width  -= adjust_w;
	sm.height -= adjust_h;
    }

    write_port(gui.vdcmp, VimMsg::Resize, &sm, sizeof(sm));
    

    return;

    
}



VimTextAreaView::VimTextAreaView(BRect frame):
    BView(frame, "VimTextAreaView", B_FOLLOW_ALL_SIDES,
#ifdef FEAT_MBYTE_IME
	B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE | B_INPUT_METHOD_AWARE
#else
	B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE
#endif
	),
    mouseDragEventCount(0)
{
#ifdef FEAT_MBYTE_IME
    IMData.messenger = NULL;
    IMData.message = NULL;
#endif
    init(frame);
}

VimTextAreaView::~VimTextAreaView()
{
    gui.vimTextArea = NULL;
}

    void
VimTextAreaView::init(BRect frame)
{
    
    gui.vimTextArea = this;

    
    SetViewColor(B_TRANSPARENT_32_BIT);
#define PEN_WIDTH   1
    SetPenSize(PEN_WIDTH);
#define W_WIDTH(curwin)   0
}

    void
VimTextAreaView::Draw(BRect updateRect)
{
    

    
    rgb_color rgb = GUI_TO_RGB(gui.back_pixel);
    SetLowColor(rgb);
    FillRect(updateRect, B_SOLID_LOW);
    gui_redraw((int) updateRect.left, (int) updateRect.top,
	    (int) (updateRect.Width() + PEN_WIDTH), (int) (updateRect.Height() + PEN_WIDTH));

    
    SetLowColor(rgb);

    if (updateRect.left < FILL_X(0))	
	FillRect(BRect(updateRect.left, updateRect.top,
		    FILL_X(0)-PEN_WIDTH, updateRect.bottom), B_SOLID_LOW);
    if (updateRect.top < FILL_Y(0)) 
	FillRect(BRect(updateRect.left, updateRect.top,
		    updateRect.right, FILL_Y(0)-PEN_WIDTH), B_SOLID_LOW);
    if (updateRect.right >= FILL_X(Columns)) 
	FillRect(BRect(FILL_X((int)Columns), updateRect.top,
		    updateRect.right, updateRect.bottom), B_SOLID_LOW);
    if (updateRect.bottom >= FILL_Y(Rows))   
	FillRect(BRect(updateRect.left, FILL_Y((int)Rows),
		    updateRect.right, updateRect.bottom), B_SOLID_LOW);

#ifdef FEAT_MBYTE_IME
    DrawIMString();
#endif
}

    void
VimTextAreaView::KeyDown(const char *bytes, int32 numBytes)
{
    struct VimKeyMsg km;
    char_u *dest = km.chars;

    bool canHaveVimModifiers = false;

    BMessage *msg = Window()->CurrentMessage();
    assert(msg);
    

    
    if (numBytes > 1) {
	
	if (numBytes > KEY_MSG_BUFSIZ)
	    numBytes = KEY_MSG_BUFSIZ;	    
	km.length = numBytes;
	memcpy((char *)dest, bytes, numBytes);
	km.csi_escape = true;
    } else {
	int32 scancode = 0;
	msg->FindInt32("key", &scancode);

	int32 beModifiers = 0;
	msg->FindInt32("modifiers", &beModifiers);

	char_u string[3];
	int len = 0;
	km.length = 0;

	
	assert(B_BACKSPACE <= 0x20);
	assert(B_DELETE == 0x7F);
	if (((char_u)bytes[0] <= 0x20 || (char_u)bytes[0] == 0x7F) &&
		numBytes == 1) {
	    
	    if (beModifiers & B_CONTROL_KEY) {
		int index = keyMap->normal_map[scancode];
		int newNumBytes = keyMapChars[index];
		char_u *newBytes = (char_u *)&keyMapChars[index + 1];

		
		if (newNumBytes != 1 || (*newBytes > 0x20 &&
			    *newBytes != 0x7F )) {
		    goto notspecial;
		}
		bytes = (char *)newBytes;
	    }
	    canHaveVimModifiers = true;

	    uint16 beoskey;
	    int first, last;

	    
	    if (numBytes == 0 || bytes[0] == B_FUNCTION_KEY) {
		beoskey = F(scancode);
		first = FIRST_FUNCTION_KEY;
		last = NUM_SPECIAL_KEYS;
	    } else if (*bytes == '\n' && scancode == 0x47) {
		
		string[0] = '\r';
		len = 1;
		first = last = 0;
	    } else {
		beoskey = K(bytes[0]);
		first = 0;
		last = FIRST_FUNCTION_KEY;
	    }

	    for (int i = first; i < last; i++) {
		if (special_keys[i].BeKeys == beoskey) {
		    string[0] = CSI;
		    string[1] = special_keys[i].vim_code0;
		    string[2] = special_keys[i].vim_code1;
		    len = 3;
		}
	    }
	}
notspecial:
	if (len == 0) {
	    string[0] = bytes[0];
	    len = 1;
	}

	
#if 0
	if (len == 3 ||
		bytes[0] == B_SPACE || bytes[0] == B_TAB ||
		bytes[0] == B_RETURN || bytes[0] == '\r' ||
		bytes[0] == B_ESCAPE)
#else
	    if (canHaveVimModifiers)
#endif
	    {
		int modifiers;
		modifiers = 0;
		if (beModifiers & B_SHIFT_KEY)
		    modifiers |= MOD_MASK_SHIFT;
		if (beModifiers & B_CONTROL_KEY)
		    modifiers |= MOD_MASK_CTRL;
		if (beModifiers & B_OPTION_KEY)
		    modifiers |= MOD_MASK_ALT;

		
		int key;
		if (string[0] == CSI && len == 3)
		    key = TO_SPECIAL(string[1], string[2]);
		else
		    key = string[0];
		key = simplify_key(key, &modifiers);
		if (IS_SPECIAL(key))
		{
		    string[0] = CSI;
		    string[1] = K_SECOND(key);
		    string[2] = K_THIRD(key);
		    len = 3;
		}
		else
		{
		    string[0] = key;
		    len = 1;
		}

		if (modifiers)
		{
		    *dest++ = CSI;
		    *dest++ = KS_MODIFIER;
		    *dest++ = modifiers;
		    km.length = 3;
		}
	    }
	memcpy((char *)dest, string, len);
	km.length += len;
	km.csi_escape = false;
    }

    write_port(gui.vdcmp, VimMsg::Key, &km, sizeof(km));

    
    if (p_mh && !gui.pointer_hidden)
    {
	guiBlankMouse(true);
	gui.pointer_hidden = TRUE;
    }
}
void
VimTextAreaView::guiSendMouseEvent(
	int	button,
	int	x,
	int	y,
	int	repeated_click,
	int_u	modifiers)
{
    VimMouseMsg mm;

    mm.button = button;
    mm.x = x;
    mm.y = y;
    mm.repeated_click = repeated_click;
    mm.modifiers = modifiers;

    write_port(gui.vdcmp, VimMsg::Mouse, &mm, sizeof(mm));
    

    
    if (gui.pointer_hidden)
    {
	guiBlankMouse(false);
	gui.pointer_hidden = FALSE;
    }
}

void
VimTextAreaView::guiMouseMoved(
	int	x,
	int	y)
{
    VimMouseMovedMsg mm;

    mm.x = x;
    mm.y = y;

    write_port(gui.vdcmp, VimMsg::MouseMoved, &mm, sizeof(mm));

    if (gui.pointer_hidden)
    {
	guiBlankMouse(false);
	gui.pointer_hidden = FALSE;
    }
}

    void
VimTextAreaView::guiBlankMouse(bool should_hide)
{
    if (should_hide) {
	
	gui.vimApp->ObscureCursor();
	
    } else {
	
    }
}

    int_u
VimTextAreaView::mouseModifiersToVim(int32 beModifiers)
{
    int_u vim_modifiers = 0x0;

    if (beModifiers & B_SHIFT_KEY)
	vim_modifiers |= MOUSE_SHIFT;
    if (beModifiers & B_CONTROL_KEY)
	vim_modifiers |= MOUSE_CTRL;
    if (beModifiers & B_OPTION_KEY)	
	vim_modifiers |= MOUSE_ALT;

    return vim_modifiers;
}

    void
VimTextAreaView::MouseDown(BPoint point)
{
    BMessage *m = Window()->CurrentMessage();
    assert(m);

    int32 buttons = 0;
    m->FindInt32("buttons", &buttons);

    int vimButton;

    if (buttons & B_PRIMARY_MOUSE_BUTTON)
	vimButton = MOUSE_LEFT;
    else if (buttons & B_SECONDARY_MOUSE_BUTTON)
	vimButton = MOUSE_RIGHT;
    else if (buttons & B_TERTIARY_MOUSE_BUTTON)
	vimButton = MOUSE_MIDDLE;
    else
	return;		

    vimMouseButton = 1;	    

    
    int32 clicks = 0;
    m->FindInt32("clicks", &clicks);

    int32 modifiers = 0;
    m->FindInt32("modifiers", &modifiers);

    vimMouseModifiers = mouseModifiersToVim(modifiers);

    guiSendMouseEvent(vimButton, point.x, point.y,
	    clicks > 1 , vimMouseModifiers);
}

    void
VimTextAreaView::MouseUp(BPoint point)
{
    vimMouseButton = 0;

    BMessage *m = Window()->CurrentMessage();
    assert(m);
    

    int32 modifiers = 0;
    m->FindInt32("modifiers", &modifiers);

    vimMouseModifiers = mouseModifiersToVim(modifiers);

    guiSendMouseEvent(MOUSE_RELEASE, point.x, point.y,
	    0 , vimMouseModifiers);

    Inherited::MouseUp(point);
}

    void
VimTextAreaView::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
    
    if (gui.pointer_hidden)
    {
	guiBlankMouse(false);
	gui.pointer_hidden = FALSE;
    }

    if (!vimMouseButton) {    
	guiMouseMoved(point.x, point.y);
	return;
    }

    atomic_add(&mouseDragEventCount, 1);

    
    guiSendMouseEvent(MOUSE_DRAG, point.x, point.y, 0, vimMouseModifiers);
}

    void
VimTextAreaView::MessageReceived(BMessage *m)
{
    switch (m->what) {
	case 'menu':
	    {
		VimMenuMsg mm;
		mm.guiMenu = NULL;  
		m->FindPointer("VimMenu", (void **)&mm.guiMenu);
		write_port(gui.vdcmp, VimMsg::Menu, &mm, sizeof(mm));
	    }
	    break;
	case B_MOUSE_WHEEL_CHANGED:
	    {
		VimScrollBar* scb = curwin->w_scrollbars[1].id;
		float small=0, big=0, dy=0;
		m->FindFloat("be:wheel_delta_y", &dy);
		scb->GetSteps(&small, &big);
		scb->SetValue(scb->Value()+small*dy*3);
		scb->ValueChanged(scb->Value());
#if 0
		scb = curwin->w_scrollbars[0].id;
		scb->GetSteps(&small, &big);
		scb->SetValue(scb->Value()+small*dy);
		scb->ValueChanged(scb->Value());
#endif
	    }
	    break;
#ifdef FEAT_MBYTE_IME
	case B_INPUT_METHOD_EVENT:
	    {
		int32 opcode;
		m->FindInt32("be:opcode", &opcode);
		switch(opcode)
		{
		    case B_INPUT_METHOD_STARTED:
			if (!IMData.messenger) delete IMData.messenger;
			IMData.messenger = new BMessenger();
			m->FindMessenger("be:reply_to", IMData.messenger);
			break;
		    case B_INPUT_METHOD_CHANGED:
			{
			    BString str;
			    bool confirmed;
			    if (IMData.message) *(IMData.message) = *m;
			    else	       IMData.message = new BMessage(*m);
			    DrawIMString();
			    m->FindBool("be:confirmed", &confirmed);
			    if (confirmed)
			    {
				m->FindString("be:string", &str);
				char_u *chars = (char_u*)str.String();
				struct VimKeyMsg km;
				km.csi_escape = true;
				int clen;
				int i = 0;
				while (i < str.Length())
				{
				    clen = utf_ptr2len(chars+i);
				    memcpy(km.chars, chars+i, clen);
				    km.length = clen;
				    write_port(gui.vdcmp, VimMsg::Key, &km, sizeof(km));
				    i += clen;
				}
			    }
			}
			break;
		    case B_INPUT_METHOD_LOCATION_REQUEST:
			{
			    BMessage msg(B_INPUT_METHOD_EVENT);
			    msg.AddInt32("be:opcode", B_INPUT_METHOD_LOCATION_REQUEST);
			    msg.AddPoint("be:location_reply", IMData.location);
			    msg.AddFloat("be:height_reply", FILL_Y(1));
			    IMData.messenger->SendMessage(&msg);
			}
			break;
		    case B_INPUT_METHOD_STOPPED:
			delete IMData.messenger;
			delete IMData.message;
			IMData.messenger = NULL;
			IMData.message = NULL;
			break;
		}
	    }
	    
#endif
	default:
	    if (m->WasDropped()) {
		BWindow *w = Window();
		w->DetachCurrentMessage();
		w->Minimize(false);
		VimApp::SendRefs(m, (modifiers() & B_SHIFT_KEY) != 0);
	    } else {
		Inherited::MessageReceived(m);
	    }
	    break;
    }
}

    int
VimTextAreaView::mchInitFont(char_u *name)
{
    VimFont *newFont = (VimFont *)gui_mch_get_font(name, 1);
    if (newFont != NOFONT) {
	gui.norm_font = (GuiFont)newFont;
	gui_mch_set_font((GuiFont)newFont);
	if (name && STRCMP(name, "*") != 0)
	    hl_set_font_name(name);

	SetDrawingMode(B_OP_COPY);

	
	return OK;
    }
    return FAIL;
}

    void
VimTextAreaView::mchDrawString(int row, int col, char_u *s, int len, int flags)
{
    
    if (!(flags & DRAW_TRANSP)) {
	int cells;
	cells = 0;
	for (int i=0; i<len; i++) {
	    int cn = utf_ptr2cells((char_u *)(s+i));
	    if (cn<4) cells += cn;
	}

	BRect r(FILL_X(col), FILL_Y(row),
		FILL_X(col + cells) - PEN_WIDTH, FILL_Y(row + 1) - PEN_WIDTH);
	FillRect(r, B_SOLID_LOW);
    }

    BFont font;
    this->GetFont(&font);
    if (!font.IsFixed())
    {
	char* p = (char*)s;
	int32 clen, lastpos = 0;
	BPoint where;
	int cells;
	while ((p - (char*)s) < len) {
	    clen = utf_ptr2len((u_char*)p);
	    where.Set(TEXT_X(col+lastpos), TEXT_Y(row));
	    DrawString(p, clen, where);
	    if (flags & DRAW_BOLD) {
		where.x += 1.0;
		SetDrawingMode(B_OP_BLEND);
		DrawString(p, clen, where);
		SetDrawingMode(B_OP_COPY);
	    }
	    cells = utf_ptr2cells((char_u *)p);
	    if (cells<4) lastpos += cells;
	    else	lastpos++;
	    p += clen;
	}
    }
    else
    {
	BPoint where(TEXT_X(col), TEXT_Y(row));
	DrawString((char*)s, len, where);
	if (flags & DRAW_BOLD) {
	    where.x += 1.0;
	    SetDrawingMode(B_OP_BLEND);
	    DrawString((char*)s, len, where);
	    SetDrawingMode(B_OP_COPY);
	}
    }

    if (flags & DRAW_UNDERL) {
	int cells;
	cells = 0;
	for (int i=0; i<len; i++) {
	    int cn = utf_ptr2cells((char_u *)(s+i));
	    if (cn<4) cells += cn;
	}

	BPoint start(FILL_X(col), FILL_Y(row + 1) - PEN_WIDTH);
	BPoint end(FILL_X(col + cells) - PEN_WIDTH, start.y);

	StrokeLine(start, end);
    }
}

void
VimTextAreaView::mchClearBlock(
	int	row1,
	int	col1,
	int	row2,
	int	col2)
{
    BRect r(FILL_X(col1), FILL_Y(row1),
	    FILL_X(col2 + 1) - PEN_WIDTH, FILL_Y(row2 + 1) - PEN_WIDTH);
    gui_mch_set_bg_color(gui.back_pixel);
    FillRect(r, B_SOLID_LOW);
}

    void
VimTextAreaView::mchClearAll()
{
    gui_mch_set_bg_color(gui.back_pixel);
    FillRect(Bounds(), B_SOLID_LOW);
}


    void
VimTextAreaView::mchDeleteLines(int row, int num_lines)
{
    BRect source, dest;
    source.left = FILL_X(gui.scroll_region_left);
    source.top = FILL_Y(row + num_lines);
    source.right = FILL_X(gui.scroll_region_right + 1) - PEN_WIDTH;
    source.bottom = FILL_Y(gui.scroll_region_bot + 1) - PEN_WIDTH;

    dest.left = FILL_X(gui.scroll_region_left);
    dest.top = FILL_Y(row);
    dest.right = FILL_X(gui.scroll_region_right + 1) - PEN_WIDTH;
    dest.bottom = FILL_Y(gui.scroll_region_bot - num_lines + 1) - PEN_WIDTH;

    if (gui.vimWindow->Lock()) {
	
	CopyBits(source, dest);
	gui_clear_block(gui.scroll_region_bot - num_lines + 1,
		gui.scroll_region_left,
		gui.scroll_region_bot, gui.scroll_region_right);


	gui.vimWindow->Unlock();
	
    }
    
    
}


    void
VimTextAreaView::mchInsertLines(int row, int num_lines)
{
    BRect source, dest;

    
    gui.vimWindow->UpdateIfNeeded();
    source.left = FILL_X(gui.scroll_region_left);
    source.top = FILL_Y(row);
    source.right = FILL_X(gui.scroll_region_right + 1) - PEN_WIDTH;
    source.bottom = FILL_Y(gui.scroll_region_bot - num_lines + 1) - PEN_WIDTH;

    dest.left = FILL_X(gui.scroll_region_left);
    dest.top = FILL_Y(row + num_lines);
    dest.right = FILL_X(gui.scroll_region_right + 1) - PEN_WIDTH;
    dest.bottom = FILL_Y(gui.scroll_region_bot + 1) - PEN_WIDTH;

    if (gui.vimWindow->Lock()) {
	
	CopyBits(source, dest);
	gui_clear_block(row, gui.scroll_region_left,
		row + num_lines - 1, gui.scroll_region_right);

	gui.vimWindow->Unlock();
	

	

    }
}

#ifdef FEAT_MBYTE_IME

void VimTextAreaView::DrawIMString(void)
{
    static const rgb_color r_highlight = {255, 152, 152, 255},
		 b_highlight = {152, 203, 255, 255};
    BString str;
    const char* s;
    int len;
    BMessage* msg = IMData.message;
    if (!msg)
	return;
    gui_redraw_block(IMData.row, 0,
	    IMData.row + IMData.count, W_WIDTH(curwin), GUI_MON_NOCLEAR);
    bool confirmed = false;
    msg->FindBool("be:confirmed", &confirmed);
    if (confirmed)
	return;
    rgb_color hcolor = HighColor(), lcolor = LowColor();
    msg->FindString("be:string", &str);
    s = str.String();
    len = str.Length();
    SetHighColor(0, 0, 0);
    IMData.row = gui.row;
    IMData.col = gui.col;
    int32 sel_start = 0, sel_end = 0;
    msg->FindInt32("be:selection", 0, &sel_start);
    msg->FindInt32("be:selection", 1, &sel_end);
    int clen, cn;
    BPoint pos(IMData.col, 0);
    BRect r;
    BPoint where;
    IMData.location = ConvertToScreen(
	    BPoint(FILL_X(pos.x), FILL_Y(IMData.row + pos.y)));
    for (int i=0; i<len; i+=clen)
    {
	cn = utf_ptr2cells((char_u *)(s+i));
	clen = utf_ptr2len((char_u *)(s+i));
	if (pos.x + cn > W_WIDTH(curwin))
	{
	    pos.y++;
	    pos.x = 0;
	}
	if (sel_start<=i && i<sel_end)
	{
	    SetLowColor(r_highlight);
	    IMData.location = ConvertToScreen(
		    BPoint(FILL_X(pos.x), FILL_Y(IMData.row + pos.y)));
	}
	else
	{
	    SetLowColor(b_highlight);
	}
	r.Set(FILL_X(pos.x), FILL_Y(IMData.row + pos.y),
		FILL_X(pos.x + cn) - PEN_WIDTH,
		FILL_Y(IMData.row + pos.y + 1) - PEN_WIDTH);
	FillRect(r, B_SOLID_LOW);
	where.Set(TEXT_X(pos.x), TEXT_Y(IMData.row + pos.y));
	DrawString((s+i), clen, where);
	pos.x += cn;
    }
    IMData.count = (int)pos.y;

    SetHighColor(hcolor);
    SetLowColor(lcolor);
}
#endif



VimScrollBar::VimScrollBar(scrollbar_T *g, orientation posture):
    BScrollBar(posture == B_HORIZONTAL ?  BRect(-100,-100,-10,-90) :
	    BRect(-100,-100,-90,-10),
	    "vim scrollbar", (BView *)NULL,
	    0.0, 10.0, posture),
    ignoreValue(-1),
    scrollEventCount(0)
{
    gsb = g;
    SetResizingMode(B_FOLLOW_NONE);
}

VimScrollBar::~VimScrollBar()
{
}

    void
VimScrollBar::ValueChanged(float newValue)
{
    if (ignoreValue >= 0.0 && newValue == ignoreValue) {
	ignoreValue = -1;
	return;
    }
    ignoreValue = -1;
    
    atomic_add(&scrollEventCount, 1);

    struct VimScrollBarMsg sm;

    sm.sb = this;
    sm.value = (long) newValue;
    sm.stillDragging = TRUE;

    write_port(gui.vdcmp, VimMsg::ScrollBar, &sm, sizeof(sm));

    
}


    void
VimScrollBar::MouseUp(BPoint where)
{
    
    

    atomic_add(&scrollEventCount, 1);

    struct VimScrollBarMsg sm;

    sm.sb = this;
    sm.value = (long) Value();
    sm.stillDragging = FALSE;

    write_port(gui.vdcmp, VimMsg::ScrollBar, &sm, sizeof(sm));

    

    Inherited::MouseUp(where);
}

    void
VimScrollBar::SetValue(float newValue)
{
    if (newValue == Value())
	return;

    ignoreValue = newValue;
    Inherited::SetValue(newValue);
}



VimFont::VimFont(): BFont()
{
    init();
}

VimFont::VimFont(const VimFont *rhs): BFont(rhs)
{
    init();
}

VimFont::VimFont(const BFont *rhs): BFont(rhs)
{
    init();
}

VimFont::VimFont(const VimFont &rhs): BFont(rhs)
{
    init();
}

VimFont::~VimFont()
{
}

    void
VimFont::init()
{
    next = NULL;
    refcount = 1;
    name = NULL;
}



#if defined(FEAT_GUI_DIALOG)

const unsigned int  kVimDialogButtonMsg = 'VMDB';
const unsigned int  kVimDialogIconStripeWidth = 30;
const unsigned int  kVimDialogButtonsSpacingX = 9;
const unsigned int  kVimDialogButtonsSpacingY = 4;
const unsigned int  kVimDialogSpacingX = 6;
const unsigned int  kVimDialogSpacingY = 10;
const unsigned int  kVimDialogMinimalWidth  = 310;
const unsigned int  kVimDialogMinimalHeight = 75;
const BRect	    kDefaultRect =
BRect(0, 0, kVimDialogMinimalWidth, kVimDialogMinimalHeight);

VimDialog::VimDialog(int type, const char *title, const char *message,
	const char *buttons, int dfltbutton, const char *textfield, int ex_cmd)
: BWindow(kDefaultRect, title, B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
	B_NOT_CLOSABLE | B_NOT_RESIZABLE |
	B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE | B_ASYNCHRONOUS_CONTROLS)
    , fDialogSem(-1)
    , fDialogValue(dfltbutton)
    , fMessageView(NULL)
    , fInputControl(NULL)
    , fInputValue(textfield)
{
    
    VimDialog::View* view = new VimDialog::View(Bounds());
    if (view == NULL)
	return;

    if (title == NULL)
	SetTitle("Vim " VIM_VERSION_MEDIUM);

    AddChild(view);

    
    view->InitIcon(type);

    
    int32 which = 1;
    float maxButtonWidth  = 0;
    float maxButtonHeight = 0;
    float buttonsWidth	  = 0;
    float buttonsHeight   = 0;
    BString strButtons(buttons);
    strButtons.RemoveAll("&");
    do {
	int32 end = strButtons.FindFirst('\n');
	if (end != B_ERROR)
	    strButtons.SetByteAt(end, '\0');

	BButton *button = _CreateButton(which++, strButtons.String());
	view->AddChild(button);
	fButtonsList.AddItem(button);

	maxButtonWidth	= max_c(maxButtonWidth,  button->Bounds().Width());
	maxButtonHeight = max_c(maxButtonHeight, button->Bounds().Height());
	buttonsWidth   += button->Bounds().Width();
	buttonsHeight  += button->Bounds().Height();

	if (end == B_ERROR)
	    break;

	strButtons.Remove(0, end + 1);
    } while (true);

    int32 buttonsCount = fButtonsList.CountItems();
    buttonsWidth      += kVimDialogButtonsSpacingX * (buttonsCount - 1);
    buttonsHeight     += kVimDialogButtonsSpacingY * (buttonsCount - 1);
    float dialogWidth  = buttonsWidth + kVimDialogIconStripeWidth +
	kVimDialogSpacingX * 2;
    float dialogHeight = maxButtonHeight + kVimDialogSpacingY * 3;

    
    bool vertical = (vim_strchr(p_go, GO_VERTICAL) != NULL) ||
	dialogWidth >= gui.vimWindow->Bounds().Width();
    if (vertical) {
	dialogWidth  -= buttonsWidth;
	dialogWidth  += maxButtonWidth;
	dialogHeight -= maxButtonHeight;
	dialogHeight += buttonsHeight;
    }

    dialogWidth  = max_c(dialogWidth,  kVimDialogMinimalWidth);

    
    BRect rect(0, 0, dialogWidth, 0);
    rect.left  += kVimDialogIconStripeWidth + 16 + kVimDialogSpacingX;
    rect.top   += kVimDialogSpacingY;
    rect.right -= kVimDialogSpacingX;
    rect.bottom = rect.top;
    fMessageView = new BTextView(rect, "_tv_", rect.OffsetByCopy(B_ORIGIN),
	    B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW);

    fMessageView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    rgb_color textColor = ui_color(B_PANEL_TEXT_COLOR);
    fMessageView->SetFontAndColor(be_plain_font, B_FONT_ALL, &textColor);
    fMessageView->SetText(message);
    fMessageView->MakeEditable(false);
    fMessageView->MakeSelectable(false);
    fMessageView->SetWordWrap(true);
    AddChild(fMessageView);

    float messageHeight = fMessageView->TextHeight(0, fMessageView->CountLines());
    fMessageView->ResizeBy(0, messageHeight);
    fMessageView->SetTextRect(BRect(0, 0, rect.Width(), messageHeight));

    dialogHeight += messageHeight;

    
    if (fInputValue != NULL) {
	rect.top     =
	    rect.bottom += messageHeight + kVimDialogSpacingY;
	fInputControl = new BTextControl(rect, "_iv_", NULL, fInputValue, NULL,
		B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW | B_NAVIGABLE |  B_PULSE_NEEDED);
	fInputControl->TextView()->SetText(fInputValue);
	fInputControl->TextView()->SetWordWrap(false);
	AddChild(fInputControl);

	float width = 0.f, height = 0.f;
	fInputControl->GetPreferredSize(&width, &height);
	fInputControl->MakeFocus(true);

	dialogHeight += height + kVimDialogSpacingY * 1.5;
    }

    dialogHeight = max_c(dialogHeight, kVimDialogMinimalHeight);

    ResizeTo(dialogWidth, dialogHeight);
    MoveTo((gui.vimWindow->Bounds().Width() - dialogWidth) / 2,
	    (gui.vimWindow->Bounds().Height() - dialogHeight) / 2);

    
    float buttonWidth = max_c(maxButtonWidth, rect.Width() * 0.66);
    BPoint origin(dialogWidth, dialogHeight);
    origin.x -= kVimDialogSpacingX + (vertical ? buttonWidth : buttonsWidth);
    origin.y -= kVimDialogSpacingY + (vertical ? buttonsHeight	: maxButtonHeight);

    for (int32 i = 0 ; i < buttonsCount; i++) {
	BButton *button = (BButton*)fButtonsList.ItemAt(i);
	button->MoveTo(origin);
	if (vertical) {
	    origin.y += button->Frame().Height() + kVimDialogButtonsSpacingY;
	    button->ResizeTo(buttonWidth, button->Frame().Height());
	} else
	    origin.x += button->Frame().Width() + kVimDialogButtonsSpacingX;

	if (dfltbutton == i + 1) {
	    button->MakeDefault(true);
	    button->MakeFocus(fInputControl == NULL);
	}
    }
}

VimDialog::~VimDialog()
{
    if (fDialogSem > B_OK)
	delete_sem(fDialogSem);
}

    int
VimDialog::Go()
{
    fDialogSem = create_sem(0, "VimDialogSem");
    if (fDialogSem < B_OK) {
	Quit();
	return fDialogValue;
    }

    Show();

    while (acquire_sem(fDialogSem) == B_INTERRUPTED);

    int retValue = fDialogValue;
    if (fInputValue != NULL)
	vim_strncpy((char_u*)fInputValue, (char_u*)fInputControl->Text(), IOSIZE - 1);

    if (Lock())
	Quit();

    return retValue;
}

void VimDialog::MessageReceived(BMessage *msg)
{
    int32 which = 0;
    if (msg->what != kVimDialogButtonMsg ||
	    msg->FindInt32("which", &which) != B_OK)
	return BWindow::MessageReceived(msg);

    fDialogValue = which;
    delete_sem(fDialogSem);
    fDialogSem = -1;
}

BButton* VimDialog::_CreateButton(int32 which, const char* label)
{
    BMessage *message = new BMessage(kVimDialogButtonMsg);
    message->AddInt32("which", which);

    BRect rect(0, 0, 0, 0);
    BString name;
    name << "_b" << which << "_";

    BButton* button = new BButton(rect, name.String(), label, message,
	    B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);

    float width = 0.f, height = 0.f;
    button->GetPreferredSize(&width, &height);
    button->ResizeTo(width, height);

    return button;
}

VimDialog::View::View(BRect frame)
    :	BView(frame, "VimDialogView", B_FOLLOW_ALL_SIDES, B_WILL_DRAW),
    fIconBitmap(NULL)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}

VimDialog::View::~View()
{
    delete fIconBitmap;
}

void VimDialog::View::Draw(BRect updateRect)
{
    BRect stripeRect = Bounds();
    stripeRect.right = kVimDialogIconStripeWidth;
    SetHighColor(tint_color(ViewColor(), B_DARKEN_1_TINT));
    FillRect(stripeRect);

    if (fIconBitmap == NULL)
	return;

    SetDrawingMode(B_OP_ALPHA);
    SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
    DrawBitmapAsync(fIconBitmap, BPoint(18, 6));
}

void VimDialog::View::InitIcon(int32 type)
{
    if (type == VIM_GENERIC)
	return;

    BPath path;
    status_t status = find_directory(B_BEOS_SERVERS_DIRECTORY, &path);
    if (status != B_OK) {
	fprintf(stderr, "Cannot retrieve app info:%s\n", strerror(status));
	return;
    }

    path.Append("app_server");

    BFile file(path.Path(), O_RDONLY);
    if (file.InitCheck() != B_OK) {
	fprintf(stderr, "App file assignment failed:%s\n",
		strerror(file.InitCheck()));
	return;
    }

    BResources resources(&file);
    if (resources.InitCheck() != B_OK) {
	fprintf(stderr, "App server resources assignment failed:%s\n",
		strerror(resources.InitCheck()));
	return;
    }

    const char *name = "";
    switch(type) {
	case VIM_ERROR:	    name = "stop"; break;
	case VIM_WARNING:   name = "warn"; break;
	case VIM_INFO:	    name = "info"; break;
	case VIM_QUESTION:  name = "idea"; break;
	default: return;
    }

    int32 iconSize = 32;
    fIconBitmap = new BBitmap(BRect(0, 0, iconSize - 1, iconSize - 1), 0, B_RGBA32);
    if (fIconBitmap == NULL || fIconBitmap->InitCheck() != B_OK) {
	fprintf(stderr, "Icon bitmap allocation failed:%s\n",
		(fIconBitmap == NULL) ? "null" : strerror(fIconBitmap->InitCheck()));
	return;
    }

    size_t size = 0;
    const uint8* iconData = NULL;
    
    iconData = (const uint8*)resources.LoadResource(B_VECTOR_ICON_TYPE, name, &size);
    if (iconData != NULL && BIconUtils::GetVectorIcon(iconData, size, fIconBitmap) == B_OK)
	return;

    
    iconData = (const uint8*)resources.LoadResource(B_LARGE_ICON_TYPE, name, &size);
    if (iconData == NULL) {
	fprintf(stderr, "Bitmap icon resource not found\n");
	delete fIconBitmap;
	fIconBitmap = NULL;
	return;
    }

    if (fIconBitmap->ColorSpace() != B_CMAP8)
	BIconUtils::ConvertFromCMAP8(iconData, iconSize, iconSize, iconSize, fIconBitmap);
}

const unsigned int  kVimDialogOKButtonMsg = 'FDOK';
const unsigned int  kVimDialogCancelButtonMsg = 'FDCN';
const unsigned int  kVimDialogSizeInputMsg = 'SICH';
const unsigned int  kVimDialogFamilySelectMsg = 'MSFM';
const unsigned int  kVimDialogStyleSelectMsg = 'MSST';
const unsigned int  kVimDialogSizeSelectMsg = 'MSSZ';

VimSelectFontDialog::VimSelectFontDialog(font_family* family, font_style* style, float* size)
: BWindow(kDefaultRect, "Font Selection", B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
	B_NOT_CLOSABLE | B_NOT_RESIZABLE |
	B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE | B_ASYNCHRONOUS_CONTROLS)
    , fStatus(B_NO_INIT)
    , fDialogSem(-1)
    , fDialogValue(false)
    , fFamily(family)
    , fStyle(style)
    , fSize(size)
    , fFontSize(*size)
    , fPreview(0)
    , fFamiliesList(0)
    , fStylesList(0)
    , fSizesList(0)
    , fSizesInput(0)
{
    strncpy(fFontFamily, *family, B_FONT_FAMILY_LENGTH);
    strncpy(fFontStyle, *style, B_FONT_STYLE_LENGTH);

    
    BBox *clientBox = new BBox(Bounds(), B_EMPTY_STRING, B_FOLLOW_ALL_SIDES,
		    B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE_JUMP | B_PULSE_NEEDED,
		    B_PLAIN_BORDER);
    AddChild(clientBox);

    
    BRect RC = clientBox->Bounds();
    RC.InsetBy(kVimDialogSpacingX, kVimDialogSpacingY);
    BRect rc(RC.LeftTop(), RC.LeftTop());

    
    fPreview = new BStringView(rc, "preview", "DejaVu Sans Mono");
    clientBox->AddChild(fPreview);

    BBox* boxDivider = new BBox(rc, B_EMPTY_STRING,
	    B_FOLLOW_NONE, B_WILL_DRAW, B_FANCY_BORDER);
    clientBox->AddChild(boxDivider);

    BStringView *labelFamily = new BStringView(rc, "labelFamily", "Family:");
    clientBox->AddChild(labelFamily);
    labelFamily->ResizeToPreferred();

    BStringView *labelStyle = new BStringView(rc, "labelStyle", "Style:");
    clientBox->AddChild(labelStyle);
    labelStyle->ResizeToPreferred();

    BStringView *labelSize = new BStringView(rc, "labelSize", "Size:");
    clientBox->AddChild(labelSize);
    labelSize->ResizeToPreferred();

    fFamiliesList = new BListView(rc, "listFamily",
	    B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES);
    BScrollView *scrollFamilies = new BScrollView("scrollFamily",
	    fFamiliesList, B_FOLLOW_LEFT_RIGHT, 0, false, true);
    clientBox->AddChild(scrollFamilies);

    fStylesList= new BListView(rc, "listStyles",
	    B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES);
    BScrollView *scrollStyles = new BScrollView("scrollStyle",
	    fStylesList, B_FOLLOW_LEFT_RIGHT, 0, false, true);
    clientBox->AddChild(scrollStyles);

    fSizesInput = new BTextControl(rc, "inputSize", NULL, "???",
	    new BMessage(kVimDialogSizeInputMsg));
    clientBox->AddChild(fSizesInput);
    fSizesInput->ResizeToPreferred();

    fSizesList = new BListView(rc, "listSizes",
	    B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL_SIDES);
    BScrollView *scrollSizes = new BScrollView("scrollSize",
	    fSizesList, B_FOLLOW_LEFT_RIGHT, 0, false, true);
    clientBox->AddChild(scrollSizes);

    BButton *buttonOK = new BButton(rc, "buttonOK", "OK",
			new BMessage(kVimDialogOKButtonMsg));
    clientBox->AddChild(buttonOK);
    buttonOK->ResizeToPreferred();

    BButton *buttonCancel = new BButton(rc, "buttonCancel", "Cancel",
			new BMessage(kVimDialogCancelButtonMsg));
    clientBox->AddChild(buttonCancel);
    buttonCancel->ResizeToPreferred();

    
    float lineHeight = labelFamily->Bounds().Height();
    float previewHeight = lineHeight * 3;
    float offsetYLabels = previewHeight + kVimDialogSpacingY;
    float offsetYLists = offsetYLabels + lineHeight + kVimDialogSpacingY / 2;
    float offsetYSizes = offsetYLists + fSizesInput->Bounds().Height() + kVimDialogSpacingY / 2;
    float listsHeight = lineHeight * 9;
    float offsetYButtons = offsetYLists + listsHeight +  kVimDialogSpacingY;
    float maxControlsHeight = offsetYButtons + buttonOK->Bounds().Height();
    float familiesWidth = labelFamily->Bounds().Width() * 5;
    float offsetXStyles = familiesWidth + kVimDialogSpacingX;
    float stylesWidth = labelStyle->Bounds().Width() * 4;
    float offsetXSizes = offsetXStyles + stylesWidth + kVimDialogSpacingX;
    float sizesWidth = labelSize->Bounds().Width() * 2;
    float maxControlsWidth = offsetXSizes + sizesWidth;

    ResizeTo(maxControlsWidth + kVimDialogSpacingX * 2,
	maxControlsHeight + kVimDialogSpacingY * 2);

    BRect rcVim = gui.vimWindow->Frame();
    MoveTo(rcVim.left + (rcVim.Width() - Frame().Width()) / 2,
	    rcVim.top + (rcVim.Height() - Frame().Height()) / 2);

    fPreview->ResizeTo(maxControlsWidth, previewHeight);
    fPreview->SetAlignment(B_ALIGN_CENTER);

    boxDivider->MoveBy(0.f, previewHeight + kVimDialogSpacingY / 2);
    boxDivider->ResizeTo(maxControlsWidth, 1.f);

    labelFamily->MoveBy(0.f, offsetYLabels);
    labelStyle->MoveBy(offsetXStyles, offsetYLabels);
    labelSize->MoveBy(offsetXSizes, offsetYLabels);

    
    float insetX = fSizesInput->TextView()->Bounds().Width() - fSizesInput->Bounds().Width();
    float insetY = fSizesInput->TextView()->Bounds().Width() - fSizesInput->Bounds().Width();

    scrollFamilies->MoveBy(0.f, offsetYLists);
    scrollStyles->MoveBy(offsetXStyles, offsetYLists);
    fSizesInput->MoveBy(offsetXSizes + insetX / 2, offsetYLists + insetY / 2);
    scrollSizes->MoveBy(offsetXSizes, offsetYSizes);

    fSizesInput->SetAlignment(B_ALIGN_CENTER, B_ALIGN_CENTER);

    scrollFamilies->ResizeTo(familiesWidth, listsHeight);
    scrollStyles->ResizeTo(stylesWidth, listsHeight);
    fSizesInput->ResizeTo(sizesWidth, fSizesInput->Bounds().Height());
    scrollSizes->ResizeTo(sizesWidth,
	    listsHeight - (offsetYSizes - offsetYLists));

    buttonOK->MoveBy(maxControlsWidth - buttonOK->Bounds().Width(), offsetYButtons);
    buttonCancel->MoveBy(maxControlsWidth - buttonOK->Bounds().Width()
	    - buttonCancel->Bounds().Width() - kVimDialogSpacingX, offsetYButtons);

    
    int selIndex = -1;
    int count = count_font_families();
    for (int i = 0; i < count; i++) {
	font_family family;
	if (get_font_family(i, &family ) == B_OK) {
	    fFamiliesList->AddItem(new BStringItem((const char*)family));
	    if (strncmp(family, fFontFamily, B_FONT_FAMILY_LENGTH) == 0)
		selIndex = i;
	}
    }

    if (selIndex >= 0) {
	fFamiliesList->Select(selIndex);
	fFamiliesList->ScrollToSelection();
    }

    _UpdateFontStyles();

    selIndex = -1;
    for (int size = 8, index = 0; size <= 18; size++, index++) {
	BString str;
	str << size;
	fSizesList->AddItem(new BStringItem(str));
	if (size == fFontSize)
	    selIndex = index;

    }

    if (selIndex >= 0) {
	fSizesList->Select(selIndex);
	fSizesList->ScrollToSelection();
    }

    fFamiliesList->SetSelectionMessage(new BMessage(kVimDialogFamilySelectMsg));
    fStylesList->SetSelectionMessage(new BMessage(kVimDialogStyleSelectMsg));
    fSizesList->SetSelectionMessage(new BMessage(kVimDialogSizeSelectMsg));
    fSizesInput->SetModificationMessage(new BMessage(kVimDialogSizeInputMsg));

    _UpdateSizeInputPreview();
    _UpdateFontPreview();

    fStatus = B_OK;
}

VimSelectFontDialog::~VimSelectFontDialog()
{
    _CleanList(fFamiliesList);
    _CleanList(fStylesList);
    _CleanList(fSizesList);

    if (fDialogSem > B_OK)
	delete_sem(fDialogSem);
}

    void
VimSelectFontDialog::_CleanList(BListView* list)
{
    while (0 < list->CountItems())
	delete (dynamic_cast<BStringItem*>(list->RemoveItem((int32)0)));
}

    bool
VimSelectFontDialog::Go()
{
    if (fStatus != B_OK) {
	Quit();
	return NOFONT;
    }

    fDialogSem = create_sem(0, "VimFontSelectDialogSem");
    if (fDialogSem < B_OK) {
	Quit();
	return fDialogValue;
    }

    Show();

    while (acquire_sem(fDialogSem) == B_INTERRUPTED);

    bool retValue = fDialogValue;

    if (Lock())
	Quit();

    return retValue;
}


void VimSelectFontDialog::_UpdateFontStyles()
{
    _CleanList(fStylesList);

    int32 selIndex = -1;
    int32 count = count_font_styles(fFontFamily);
    for (int32 i = 0; i < count; i++) {
	font_style style;
	uint32 flags = 0;
	if (get_font_style(fFontFamily, i, &style, &flags) == B_OK) {
	    fStylesList->AddItem(new BStringItem((const char*)style));
	    if (strncmp(style, fFontStyle, B_FONT_STYLE_LENGTH) == 0)
		selIndex = i;
	}
    }

    if (selIndex >= 0) {
	fStylesList->Select(selIndex);
	fStylesList->ScrollToSelection();
    } else
	fStylesList->Select(0);
}


void VimSelectFontDialog::_UpdateSizeInputPreview()
{
    char buf[10] = {0};
    vim_snprintf(buf, sizeof(buf), (char*)"%.0f", fFontSize);
    fSizesInput->SetText(buf);
}


void VimSelectFontDialog::_UpdateFontPreview()
{
    BFont font;
    fPreview->GetFont(&font);
    font.SetSize(fFontSize);
    font.SetFamilyAndStyle(fFontFamily, fFontStyle);
    fPreview->SetFont(&font, B_FONT_FAMILY_AND_STYLE | B_FONT_SIZE);

    BString str;
    str << fFontFamily << " " << fFontStyle << ", " << (int)fFontSize << " pt.";
    fPreview->SetText(str);
}


    bool
VimSelectFontDialog::_UpdateFromListItem(BListView* list, char* text, int textSize)
{
    int32 index = list->CurrentSelection();
    if (index < 0)
	return false;
    BStringItem* item = (BStringItem*)list->ItemAt(index);
    if (item == NULL)
	return false;
    strncpy(text, item->Text(), textSize);
    return true;
}


void VimSelectFontDialog::MessageReceived(BMessage *msg)
{
    switch (msg->what) {
	case kVimDialogOKButtonMsg:
	    strncpy(*fFamily, fFontFamily, B_FONT_FAMILY_LENGTH);
	    strncpy(*fStyle, fFontStyle, B_FONT_STYLE_LENGTH);
	    *fSize = fFontSize;
	    fDialogValue = true;
	case kVimDialogCancelButtonMsg:
	    delete_sem(fDialogSem);
	    fDialogSem = -1;
	    return;
	case B_KEY_UP:
	    {
		int32 key = 0;
		if (msg->FindInt32("raw_char", &key) == B_OK
			&& key == B_ESCAPE) {
		    delete_sem(fDialogSem);
		    fDialogSem = -1;
		}
	    }
	    break;

	case kVimDialogFamilySelectMsg:
	    if (_UpdateFromListItem(fFamiliesList,
		    fFontFamily, B_FONT_FAMILY_LENGTH)) {
		_UpdateFontStyles();
		_UpdateFontPreview();
	    }
	    break;
	case kVimDialogStyleSelectMsg:
	    if (_UpdateFromListItem(fStylesList,
		    fFontStyle, B_FONT_STYLE_LENGTH))
		_UpdateFontPreview();
	    break;
	case kVimDialogSizeSelectMsg:
	    {
		char buf[10] = {0};
		if (_UpdateFromListItem(fSizesList, buf, sizeof(buf))) {
		    float size = atof(buf);
		    if (size > 0.f) {
			fFontSize = size;
			_UpdateSizeInputPreview();
			_UpdateFontPreview();
		    }
		}
	    }
	    break;
	case kVimDialogSizeInputMsg:
	    {
		float size = atof(fSizesInput->Text());
		if (size > 0.f) {
		    fFontSize = size;
		    _UpdateFontPreview();
		}
	    }
	    break;
	default:
	    break;
    }
    return BWindow::MessageReceived(msg);
}

#endif 

#ifdef FEAT_TOOLBAR


static BMessage * MenuMessage(vimmenu_T *menu);

VimToolbar::VimToolbar(BRect frame, const char *name) :
    BBox(frame, name, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW | B_FRAME_EVENTS, B_PLAIN_BORDER)
{
}

VimToolbar::~VimToolbar()
{
    int32 count = fButtonsList.CountItems();
    for (int32 i = 0; i < count; i++)
	delete (BPictureButton*)fButtonsList.ItemAt(i);
    fButtonsList.MakeEmpty();

    delete normalButtonsBitmap;
    delete grayedButtonsBitmap;
    normalButtonsBitmap    = NULL;
    grayedButtonsBitmap  = NULL;
}

    void
VimToolbar::AttachedToWindow()
{
    BBox::AttachedToWindow();

    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}

    float
VimToolbar::ToolbarHeight() const
{
    float size = NULL == normalButtonsBitmap ? 18. : normalButtonsBitmap->Bounds().Height();
    return size + ToolbarMargin * 2 + ButtonMargin * 2 + 1;
}

    bool
VimToolbar::ModifyBitmapToGrayed(BBitmap *bitmap)
{
    float height = bitmap->Bounds().Height();
    float width  = bitmap->Bounds().Width();

    rgb_color *bits = (rgb_color*)bitmap->Bits();
    int32 pixels = bitmap->BitsLength() / 4;
    for (int32 i = 0; i < pixels; i++) {
	bits[i].red = bits[i].green =
	bits[i].blue = ((uint32)bits[i].red + bits[i].green + bits[i].blue) / 3;
	bits[i].alpha /= 4;
    }

    return true;
}

    bool
VimToolbar::PrepareButtonBitmaps()
{
    
    normalButtonsBitmap = LoadVimBitmap("builtin-tools.png");
    if (normalButtonsBitmap == NULL)
	
	normalButtonsBitmap = BTranslationUtils::GetBitmap(B_PNG_FORMAT, "builtin-tools");

    if (normalButtonsBitmap == NULL)
	return false;

    BMessage archive;
    normalButtonsBitmap->Archive(&archive);

    grayedButtonsBitmap = new BBitmap(&archive);
    if (grayedButtonsBitmap == NULL)
	return false;

    
    ModifyBitmapToGrayed(grayedButtonsBitmap);

    return true;
}

BBitmap *VimToolbar::LoadVimBitmap(const char* fileName)
{
    BBitmap *bitmap = NULL;

    int mustfree = 0;
    char_u* runtimePath = vim_getenv((char_u*)"VIMRUNTIME", &mustfree);
    if (runtimePath != NULL && fileName != NULL) {
	BString strPath((char*)runtimePath);
	strPath << "/bitmaps/" << fileName;
	bitmap = BTranslationUtils::GetBitmap(strPath.String());
    }

    if (mustfree)
	vim_free(runtimePath);

    return bitmap;
}

    bool
VimToolbar::GetPictureFromBitmap(BPicture *pictureTo, int32 index, BBitmap *bitmapFrom, bool pressed)
{
    float size = bitmapFrom->Bounds().Height() + 1.;

    BView view(BRect(0, 0, size, size), "", 0, 0);

    AddChild(&view);
    view.BeginPicture(pictureTo);

    view.SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    view.FillRect(view.Bounds());
    view.SetDrawingMode(B_OP_OVER);

    BRect source(0, 0, size - 1, size - 1);
    BRect destination(source);

    source.OffsetBy(size * index, 0);
    destination.OffsetBy(ButtonMargin, ButtonMargin);

    view.DrawBitmap(bitmapFrom, source, destination);

    if (pressed)	{
	rgb_color shineColor  = ui_color(B_SHINE_COLOR);
	rgb_color shadowColor = ui_color(B_SHADOW_COLOR);
	size += ButtonMargin * 2 - 1;
	view.BeginLineArray(4);
	view.AddLine(BPoint(0, 0),	 BPoint(size, 0),    shadowColor);
	view.AddLine(BPoint(size, 0),	 BPoint(size, size), shineColor);
	view.AddLine(BPoint(size, size), BPoint(0, size),    shineColor);
	view.AddLine(BPoint(0, size),	 BPoint(0, 0),	     shadowColor);
	view.EndLineArray();
    }

    view.EndPicture();
    RemoveChild(&view);

    return true;
}

    bool
VimToolbar::AddButton(int32 index, vimmenu_T *menu)
{
    BPictureButton *button = NULL;
    if (!menu_is_separator(menu->name)) {
	float size = normalButtonsBitmap ?
	    normalButtonsBitmap->Bounds().Height() + 1. + ButtonMargin * 2 : 18.;
	BRect frame(0, 0, size, size);
	BPicture pictureOn;
	BPicture pictureOff;
	BPicture pictureGray;

	if (menu->iconfile == NULL && menu->iconidx >= 0 && normalButtonsBitmap) {
	    GetPictureFromBitmap(&pictureOn,  menu->iconidx, normalButtonsBitmap, true);
	    GetPictureFromBitmap(&pictureOff, menu->iconidx, normalButtonsBitmap, false);
	    GetPictureFromBitmap(&pictureGray, menu->iconidx, grayedButtonsBitmap, false);
	} else {

	    char_u buffer[MAXPATHL] = {0};
	    BBitmap *bitmap = NULL;

	    if (menu->iconfile) {
		gui_find_iconfile(menu->iconfile, buffer, (char*)"png");
		bitmap = BTranslationUtils::GetBitmap((char*)buffer);
	    }

	    if (bitmap == NULL && gui_find_bitmap(menu->name, buffer, (char*)"png") == OK)
		bitmap = BTranslationUtils::GetBitmap((char*)buffer);

	    if (bitmap == NULL)
		bitmap = new BBitmap(BRect(0, 0, size, size), B_RGB32);

	    GetPictureFromBitmap(&pictureOn,   0, bitmap, true);
	    GetPictureFromBitmap(&pictureOff,  0, bitmap, false);
	    ModifyBitmapToGrayed(bitmap);
	    GetPictureFromBitmap(&pictureGray, 0, bitmap, false);

	    delete bitmap;
	}

	button = new BPictureButton(frame, (char*)menu->name,
		    &pictureOff, &pictureOn, MenuMessage(menu));

	button->SetDisabledOn(&pictureGray);
	button->SetDisabledOff(&pictureGray);

	button->SetTarget(gui.vimTextArea);

	AddChild(button);

	menu->button = button;
    }

    bool result = fButtonsList.AddItem(button, index);
    InvalidateLayout();
    return result;
}

    bool
VimToolbar::RemoveButton(vimmenu_T *menu)
{
    if (menu->button) {
	if (fButtonsList.RemoveItem(menu->button)) {
	    delete menu->button;
	    menu->button = NULL;
	}
    }
    return true;
}

    bool
VimToolbar::GrayButton(vimmenu_T *menu, int grey)
{
    if (menu->button) {
	int32 index = fButtonsList.IndexOf(menu->button);
	if (index >= 0)
	    menu->button->SetEnabled(grey ? false : true);
    }
    return true;
}

    void
VimToolbar::InvalidateLayout()
{
    int32 offset = ToolbarMargin;
    int32 count = fButtonsList.CountItems();
    for (int32 i = 0; i < count; i++) {
	BPictureButton *button = (BPictureButton *)fButtonsList.ItemAt(i);
	if (button) {
	    button->MoveTo(offset, ToolbarMargin);
	    offset += button->Bounds().Width() + ToolbarMargin;
	} else
	    offset += ToolbarMargin * 3;
    }
}

#endif 

#if defined(FEAT_GUI_TABLINE)

    float
VimTabLine::TablineHeight() const
{


    return TabHeight(); 
}

void
VimTabLine::MouseDown(BPoint point)
{
    if (!gui_mch_showing_tabline())
	return;

    BMessage *m = Window()->CurrentMessage();
    assert(m);

    int32 buttons = 0;
    m->FindInt32("buttons", &buttons);

    int32 clicks = 0;
    m->FindInt32("clicks", &clicks);

    int index = 0; 
    for (int i = 0; i < CountTabs(); i++) {
	if (TabFrame(i).Contains(point)) {
	    index = i + 1; 
	    break;
	}
    }

    int event = -1;

    if ((buttons & B_PRIMARY_MOUSE_BUTTON) && clicks > 1)
	
	event = TABLINE_MENU_NEW;

    else if (buttons & B_TERTIARY_MOUSE_BUTTON)
	
	
	event = index > 0 ? TABLINE_MENU_CLOSE : TABLINE_MENU_NEW;

    else if (buttons & B_SECONDARY_MOUSE_BUTTON) {
	
	BPopUpMenu* popUpMenu = new BPopUpMenu("tabLineContextMenu", false, false);
	popUpMenu->AddItem(new BMenuItem(_("Close tabi R"), new BMessage(TABLINE_MENU_CLOSE)));
	popUpMenu->AddItem(new BMenuItem(_("New tab    T"), new BMessage(TABLINE_MENU_NEW)));
	popUpMenu->AddItem(new BMenuItem(_("Open tab..."), new BMessage(TABLINE_MENU_OPEN)));

	ConvertToScreen(&point);
	BMenuItem* item = popUpMenu->Go(point);
	if (item != NULL) {
	    event = item->Command();
	}

	delete popUpMenu;

    } else {
	
	BTabView::MouseDown(point);
	return;
    }

    if (event < 0)
	return;

    VimTablineMenuMsg tmm;
    tmm.index = index;
    tmm.event = event;
    write_port(gui.vdcmp, VimMsg::TablineMenu, &tmm, sizeof(tmm));
}

void
VimTabLine::VimTab::Select(BView* owner)
{
    BTab::Select(owner);

    VimTabLine *tabLine = gui.vimForm->TabLine();
    if (tabLine != NULL) {

	int32 i = 0;
	for (; i < tabLine->CountTabs(); i++)
	    if (this == tabLine->TabAt(i))
		break;


	if (i < tabLine->CountTabs()) {
	    VimTablineMsg tm;
	    tm.index = i + 1;
	    write_port(gui.vdcmp, VimMsg::Tabline, &tm, sizeof(tm));
	}
    }
}

#endif 




static char appsig[] = "application/x-vnd.Haiku-Vim-8";
key_map *keyMap;
char *keyMapChars;
int main_exitcode = 127;

    status_t
gui_haiku_process_event(bigtime_t timeout)
{
    struct VimMsg vm;
    int32 what;
    ssize_t size;

    size = read_port_etc(gui.vdcmp, &what, &vm, sizeof(vm),
	    B_TIMEOUT, timeout);

    if (size >= 0) {
	switch (what) {
	    case VimMsg::Key:
		{
		    char_u *string = vm.u.Key.chars;
		    int len = vm.u.Key.length;
		    if (len == 1 && string[0] == Ctrl_chr('C')) {
			trash_input_buf();
			got_int = TRUE;
		    }

		    if (vm.u.Key.csi_escape)
#ifndef FEAT_MBYTE_IME
		    {
			int	i;
			char_u	buf[2];

			for (i = 0; i < len; ++i)
			{
			    add_to_input_buf(string + i, 1);
			    if (string[i] == CSI)
			    {
				
				buf[0] = KS_EXTRA;
				buf[1] = (int)KE_CSI;
				add_to_input_buf(buf, 2);
			    }
			}
		    }
#else
			add_to_input_buf_csi(string, len);
#endif
		    else
			add_to_input_buf(string, len);
		}
		break;
	    case VimMsg::Resize:
		gui_resize_shell(vm.u.NewSize.width, vm.u.NewSize.height);
		break;
	    case VimMsg::ScrollBar:
		{
		    
		    int32 oldCount =
			atomic_add(&vm.u.Scroll.sb->scrollEventCount, -1);
		    if (oldCount <= 1 || !vm.u.Scroll.stillDragging)
			gui_drag_scrollbar(vm.u.Scroll.sb->getGsb(),
				vm.u.Scroll.value, vm.u.Scroll.stillDragging);
		}
		break;
#if defined(FEAT_MENU)
	    case VimMsg::Menu:
		gui_menu_cb(vm.u.Menu.guiMenu);
		break;
#endif
	    case VimMsg::Mouse:
		{
		    int32 oldCount;
		    if (vm.u.Mouse.button == MOUSE_DRAG)
			oldCount =
			    atomic_add(&gui.vimTextArea->mouseDragEventCount, -1);
		    else
			oldCount = 0;
		    if (oldCount <= 1)
			gui_send_mouse_event(vm.u.Mouse.button, vm.u.Mouse.x,
				vm.u.Mouse.y, vm.u.Mouse.repeated_click,
				vm.u.Mouse.modifiers);
		}
		break;
	    case VimMsg::MouseMoved:
		{
		    gui_mouse_moved(vm.u.MouseMoved.x, vm.u.MouseMoved.y);
		}
		break;
	    case VimMsg::Focus:
		gui.in_focus = vm.u.Focus.active;
		
		
		
		if (gui.dragged_sb) {
		    gui.dragged_sb = SBAR_NONE;
		}
		
		break;
	    case VimMsg::Refs:
		::RefsReceived(vm.u.Refs.message, vm.u.Refs.changedir);
		break;
	    case VimMsg::Tabline:
		send_tabline_event(vm.u.Tabline.index);
		break;
	    case VimMsg::TablineMenu:
		send_tabline_menu_event(vm.u.TablineMenu.index, vm.u.TablineMenu.event);
		break;
	    default:
		
		break;
	}
    }

    
    return size;
}



    int
vim_lock_screen()
{
    return !gui.vimWindow || gui.vimWindow->Lock();
}

    void
vim_unlock_screen()
{
    if (gui.vimWindow)
	gui.vimWindow->Unlock();
}

#define RUN_BAPPLICATION_IN_NEW_THREAD	0

#if RUN_BAPPLICATION_IN_NEW_THREAD

    int32
run_vimapp(void *args)
{
    VimApp app(appsig);

    gui.vimApp = &app;
    app.Run();		    

    return 0;
}

#else

    int32
call_main(void *args)
{
    struct MainArgs *ma = (MainArgs *)args;

    return main(ma->argc, ma->argv);
}
#endif


    void
gui_mch_prepare(
	int	*argc,
	char	**argv)
{
    
    if (!gui.vimApp) {
	thread_info tinfo;
	get_thread_info(find_thread(NULL), &tinfo);

	
	gui.vdcmp = create_port(B_MAX_PORT_COUNT, "vim VDCMP");

#if RUN_BAPPLICATION_IN_NEW_THREAD
	thread_id tid = spawn_thread(run_vimapp, "vim VimApp",
		tinfo.priority, NULL);
	if (tid >= B_OK) {
	    resume_thread(tid);
	} else {
	    getout(1);
	}
#else
	MainArgs ma = { *argc, argv };
	thread_id tid = spawn_thread(call_main, "vim main()",
		tinfo.priority, &ma);
	if (tid >= B_OK) {
	    VimApp app(appsig);

	    gui.vimApp = &app;
	    resume_thread(tid);
	    
	    app.Run();		    
	    
	    status_t dummy_exitcode;
	    (void)wait_for_thread(tid, &dummy_exitcode);

	    
	    exit(main_exitcode);
	}
#endif
    }
    
    
    gui.dofork = FALSE;
    
    if (!isatty(0)) {
	struct stat stat_stdin, stat_dev_null;

	if (fstat(0, &stat_stdin) == -1 ||
		stat("/dev/null", &stat_dev_null) == -1 ||
		(stat_stdin.st_dev == stat_dev_null.st_dev &&
		 stat_stdin.st_ino == stat_dev_null.st_ino))
	    gui.starting = TRUE;
    }
}


    int
gui_mch_init_check(void)
{
    return OK;	    
}


    int
gui_mch_init()
{
    display_errors();
    gui.def_norm_pixel = RGB(0x00, 0x00, 0x00);	
    gui.def_back_pixel = RGB(0xFF, 0xFF, 0xFF);	
    gui.norm_pixel = gui.def_norm_pixel;
    gui.back_pixel = gui.def_back_pixel;

    gui.scrollbar_width = (int) B_V_SCROLL_BAR_WIDTH;
    gui.scrollbar_height = (int) B_H_SCROLL_BAR_HEIGHT;
#ifdef FEAT_MENU
    gui.menu_height = 19;   
    
#endif
    gui.border_offset = 3;  

    if (gui.vdcmp < B_OK)
	return FAIL;
    get_key_map(&keyMap, &keyMapChars);

    gui.vimWindow = new VimWindow();	
    if (!gui.vimWindow)
	return FAIL;

    gui.vimWindow->Run();	

    
    
    set_normal_colors();

    
    gui_check_colors();

    
    
    highlight_gui_started();	    

    gui_mch_new_colors();	

    return OK;
}


    void
gui_mch_new_colors()
{
    rgb_color rgb = GUI_TO_RGB(gui.back_pixel);

    if (gui.vimWindow->Lock()) {
	gui.vimForm->SetViewColor(rgb);
	
	gui.vimForm->Invalidate();
	gui.vimWindow->Unlock();
    }
}


    int
gui_mch_open()
{
    if (gui_win_x != -1 && gui_win_y != -1)
	gui_mch_set_winpos(gui_win_x, gui_win_y);

    
    if (gui.vimWindow->Lock()) {
	gui.vimWindow->Show();
	gui.vimWindow->Unlock();
	return OK;
    }

    return FAIL;
}

    void
gui_mch_exit(int vim_exitcode)
{
    if (gui.vimWindow) {
	thread_id tid = gui.vimWindow->Thread();
	gui.vimWindow->Lock();
	gui.vimWindow->Quit();
	
	int32 exitcode;
	wait_for_thread(tid, &exitcode);
    }
    delete_port(gui.vdcmp);
#if !RUN_BAPPLICATION_IN_NEW_THREAD
    
#endif
    if (gui.vimApp) {
	VimTextAreaView::guiBlankMouse(false);

	main_exitcode = vim_exitcode;
#if RUN_BAPPLICATION_IN_NEW_THREAD
	thread_id tid = gui.vimApp->Thread();
	int32 exitcode;
	gui.vimApp->Lock();
	gui.vimApp->Quit();
	gui.vimApp->Unlock();
	wait_for_thread(tid, &exitcode);
#else
	gui.vimApp->Lock();
	gui.vimApp->Quit();
	gui.vimApp->Unlock();
	
	exit_thread(vim_exitcode);
#endif
    }
    
}


    int
gui_mch_get_winpos(int *x, int *y)
{
    if (gui.vimWindow->Lock()) {
	BRect r;
	r = gui.vimWindow->Frame();
	gui.vimWindow->Unlock();
	*x = (int)r.left;
	*y = (int)r.top;
	return OK;
    }
    else
	return FAIL;
}


    void
gui_mch_set_winpos(int x, int y)
{
    if (gui.vimWindow->Lock()) {
	gui.vimWindow->MoveTo(x, y);
	gui.vimWindow->Unlock();
    }
}


void
gui_mch_set_shellsize(
	int	width,
	int	height,
	int	min_width,
	int	min_height,
	int	base_width,
	int	base_height,
	int	direction) 
{
    
    if (gui.vimWindow->Lock()) {
	gui.vimWindow->ResizeTo(width - PEN_WIDTH, height - PEN_WIDTH);

	
	float minWidth, maxWidth, minHeight, maxHeight;

	gui.vimWindow->GetSizeLimits(&minWidth, &maxWidth,
		&minHeight, &maxHeight);
	gui.vimWindow->SetSizeLimits(min_width, maxWidth,
		min_height, maxHeight);

	
	gui.vimWindow->SetWindowAlignment(
		B_PIXEL_ALIGNMENT,	
		1,		
		0,		
		gui.char_width,	    
		base_width,	    
		1,		
		0,		
		gui.char_height,	
		base_height	    
		);

	gui.vimWindow->Unlock();
    }
}

void
gui_mch_get_screen_dimensions(
	int	*screen_w,
	int	*screen_h)
{
    BRect frame;

    {
	BScreen screen(gui.vimWindow);

	if (screen.IsValid()) {
	    frame = screen.Frame();
	} else {
	    frame.right = 640;
	    frame.bottom = 480;
	}
    }

    
    *screen_w = (int) frame.right - 2 * gui.scrollbar_width - 20;
    *screen_h = (int) frame.bottom - gui.scrollbar_height
#ifdef FEAT_MENU
	- gui.menu_height
#endif
	- 30;
}

void
gui_mch_set_text_area_pos(
	int	x,
	int	y,
	int	w,
	int	h)
{
    if (!gui.vimTextArea)
	return;

    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->MoveTo(x, y);
	gui.vimTextArea->ResizeTo(w - PEN_WIDTH, h - PEN_WIDTH);

#ifdef FEAT_GUI_TABLINE
	if (gui.vimForm->TabLine() != NULL) {
	    gui.vimForm->TabLine()->ResizeTo(w, gui.vimForm->TablineHeight());
	}
#endif 

	gui.vimWindow->Unlock();
    }
}




void
gui_mch_enable_scrollbar(
	scrollbar_T *sb,
	int	flag)
{
    VimScrollBar *vsb = sb->id;
    if (gui.vimWindow->Lock()) {
	
	if (flag) {
	    if (vsb->IsHidden()) {
		vsb->Show();
	    }
	} else {
	    if (!vsb->IsHidden()) {
		vsb->Hide();
	    }
	}
	gui.vimWindow->Unlock();
    }
}

void
gui_mch_set_scrollbar_thumb(
	scrollbar_T *sb,
	int	val,
	int	size,
	int	max)
{
    if (gui.vimWindow->Lock()) {
	VimScrollBar *s = sb->id;
	if (max == 0) {
	    s->SetValue(0);
	    s->SetRange(0.0, 0.0);
	} else {
	    s->SetProportion((float)size / (max + 1.0));
	    s->SetSteps(1.0, size > 5 ? size - 2 : size);
#ifndef SCROLL_PAST_END	    
	    max = max + 1 - size;
#endif
	    if (max < s->Value()) {
		
		s->SetValue(val);
		s->SetRange(0.0, max);
	    } else {
		
		s->SetRange(0.0, max);
		s->SetValue(val);
	    }
	}
	gui.vimWindow->Unlock();
    }
}

void
gui_mch_set_scrollbar_pos(
	scrollbar_T *sb,
	int	x,
	int	y,
	int	w,
	int	h)
{
    if (gui.vimWindow->Lock()) {
	BRect winb = gui.vimWindow->Bounds();
	float vsbx = x, vsby = y;
	VimScrollBar *vsb = sb->id;
	vsb->ResizeTo(w - PEN_WIDTH, h - PEN_WIDTH);
	if (winb.right-(x+w)<w) vsbx = winb.right - (w - PEN_WIDTH);
	vsb->MoveTo(vsbx, vsby);
	gui.vimWindow->Unlock();
    }
}

    int
gui_mch_get_scrollbar_xpadding(void)
{
    
    
    return 0;
}

    int
gui_mch_get_scrollbar_ypadding(void)
{
    
    
    return 0;
}

void
gui_mch_create_scrollbar(
	scrollbar_T *sb,
	int	orient)	    
{
    orientation posture =
	(orient == SBAR_HORIZ) ? B_HORIZONTAL : B_VERTICAL;

    VimScrollBar *vsb = sb->id = new VimScrollBar(sb, posture);
    if (gui.vimWindow->Lock()) {
	vsb->SetTarget(gui.vimTextArea);
	vsb->Hide();
	gui.vimForm->AddChild(vsb);
	gui.vimWindow->Unlock();
    }
}

#if defined(FEAT_WINDOWS) || defined(FEAT_GUI_HAIKU) || defined(PROTO)
void
gui_mch_destroy_scrollbar(
	scrollbar_T *sb)
{
    if (gui.vimWindow->Lock()) {
	sb->id->RemoveSelf();
	delete sb->id;
	gui.vimWindow->Unlock();
    }
}
#endif


    int
gui_mch_is_blink_off(void)
{
    return FALSE;
}



#define BLINK_NONE  0
#define BLINK_OFF   1
#define BLINK_ON    2

static int	blink_state = BLINK_NONE;
static long_u	    blink_waittime = 700;
static long_u	    blink_ontime = 400;
static long_u	    blink_offtime = 250;
static int  blink_timer = 0;

void
gui_mch_set_blinking(
	long	waittime,
	long	on,
	long	off)
{
    
    blink_waittime = waittime;
    blink_ontime = on;
    blink_offtime = off;
}


    void
gui_mch_stop_blink(int may_call_gui_update_cursor)
{
    
    if (blink_timer != 0)
    {
	
	blink_timer = 0;
    }
    if (blink_state == BLINK_OFF)
	gui_update_cursor(TRUE, FALSE);
    blink_state = BLINK_NONE;
}


    void
gui_mch_start_blink()
{
    
    if (blink_timer != 0)
	;
    
    if (blink_waittime && blink_ontime && blink_offtime && gui.in_focus)
    {
	blink_timer = 1; 
	blink_state = BLINK_ON;
	gui_update_cursor(TRUE, FALSE);
    }
}


int
gui_mch_init_font(
	char_u	    *font_name,
	int	    fontset)
{
    if (gui.vimWindow->Lock())
    {
	int rc = gui.vimTextArea->mchInitFont(font_name);
	gui.vimWindow->Unlock();

	return rc;
    }

    return FAIL;
}


    int
gui_mch_adjust_charsize()
{
    return FAIL;
}


    int
gui_mch_font_dialog(font_family* family, font_style* style, float* size)
{
#if defined(FEAT_GUI_DIALOG)
	
    VimSelectFontDialog *dialog = new VimSelectFontDialog(family, style, size);
    return dialog->Go();
#else
    return NOFONT;
#endif 
}


GuiFont
gui_mch_get_font(
	char_u	    *name,
	int	    giveErrorIfMissing)
{
    static VimFont *fontList = NULL;

    if (!gui.in_use)	
	return NOFONT;

    
    const int buff_size = B_FONT_FAMILY_LENGTH + B_FONT_STYLE_LENGTH + 20;
    static char font_name[buff_size] = {0};
    font_family family = {0};
    font_style	style  = {0};
    float size = 0.f;

    if (name == 0 && be_fixed_font == 0) {
	if (giveErrorIfMissing)
	    semsg(_(e_unknown_font_str), name);
	return NOFONT;
    }

    bool useSelectGUI = false;
    if (name != NULL)
	if (STRCMP(name, "*") == 0) {
	    useSelectGUI = true;
	    STRNCPY(font_name, hl_get_font_name(), buff_size);
	} else
	    STRNCPY(font_name, name, buff_size);

    if (font_name[0] == 0) {
	be_fixed_font->GetFamilyAndStyle(&family, &style);
	size = be_fixed_font->Size();
	vim_snprintf(font_name, buff_size,
	    (char*)"%s/%s/%.0f", family, style, size);
    }

    
    char* end = 0;
    while (end = strchr((char *)font_name, '_'))
	*end = ' ';

    
    static char buff[buff_size] = {0};
    STRNCPY(buff, font_name, buff_size);
    STRNCPY(family, strtok(buff, "/\0"), B_FONT_FAMILY_LENGTH);
    char* style_s = strtok(0, "/\0");
    if (style_s != 0)
	STRNCPY(style, style_s, B_FONT_STYLE_LENGTH);
    size = atof((style_s != 0) ? strtok(0, "/\0") : "0");

    if (useSelectGUI) {
	if (gui_mch_font_dialog(&family, &style, &size) == NOFONT)
	    return FAIL;
	
	vim_snprintf(font_name, buff_size,
		(char*)"%s/%s/%.0f", family, style, size);
	hl_set_font_name((char_u*)font_name);

	
	char_u* new_p_guifont = (char_u*)alloc(STRLEN(font_name) + 1);
	if (new_p_guifont != NULL) {
	    STRCPY(new_p_guifont, font_name);
	    vim_free(p_guifont);
	    p_guifont = new_p_guifont;
	    
	    for ( ; *new_p_guifont; ++new_p_guifont)
		if (*new_p_guifont == ' ')
		    *new_p_guifont = '_';
	}
    }

    VimFont *flp;
    for (flp = fontList; flp; flp = flp->next) {
	if (STRCMP(font_name, flp->name) == 0) {
	    flp->refcount++;
	    return (GuiFont)flp;
	}
    }

    VimFont *font = new VimFont();
    font->name = vim_strsave((char_u*)font_name);

    if (count_font_styles(family) <= 0) {
	if (giveErrorIfMissing)
	    semsg(_(e_unknown_font_str), font->name);
	delete font;
	return NOFONT;
    }

    
    font->next = fontList;
    fontList = font;

    font->SetFamilyAndStyle(family, style);
    if (size > 0.f)
	font->SetSize(size);

    font->SetSpacing(B_FIXED_SPACING);
    font->SetEncoding(B_UNICODE_UTF8);

    return (GuiFont)font;
}


void
gui_mch_set_font(
	GuiFont	font)
{
    if (gui.vimWindow->Lock()) {
	VimFont *vf = (VimFont *)font;

	gui.vimTextArea->SetFont(vf);

	gui.char_width = (int) vf->StringWidth("n");
	font_height fh;
	vf->GetHeight(&fh);
	gui.char_height = (int)(fh.ascent + 0.9999)
	    + (int)(fh.descent + 0.9999) + (int)(fh.leading + 0.9999);
	gui.char_ascent = (int)(fh.ascent + 0.9999);

	gui.vimWindow->Unlock();
    }
}


void
gui_mch_free_font(
	GuiFont	font)
{
    if (font == NOFONT)
	return;
    VimFont *f = (VimFont *)font;
    if (--f->refcount <= 0) {
	if (f->refcount < 0)
	    fprintf(stderr, "VimFont: refcount < 0\n");
	delete f;
    }
}

    char_u *
gui_mch_get_fontname(GuiFont font, char_u *name)
{
    if (name == NULL)
	return NULL;
    return vim_strsave(name);
}


    int
gui_mch_adjust_charheight()
{

    










    {
	VimFont *font = (VimFont *)gui.norm_font;
	font_height fh = {0};
	font->GetHeight(&fh);
	gui.char_height = (int)(fh.ascent + fh.descent + 0.5) + p_linespace;
	gui.char_ascent = (int)(fh.ascent + 0.5) + p_linespace / 2;
    }
    return OK;
}

    void
gui_mch_getmouse(int *x, int *y)
{
    fprintf(stderr, "gui_mch_getmouse");

     {
	 *x = -1;
	 *y = -1;
     }
}

    void
gui_mch_mousehide(int hide)
{
    fprintf(stderr, "gui_mch_getmouse");
    
}

    static int
hex_digit(int c)
{
    if (isdigit(c))
	return c - '0';
    c = TOLOWER_ASC(c);
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    return -1000;
}


guicolor_T
gui_mch_get_color(
	char_u	*name)
{
    return gui_get_color_cmn(name);
}


void
gui_mch_set_fg_color(
	guicolor_T  color)
{
    rgb_color rgb = GUI_TO_RGB(color);
    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->SetHighColor(rgb);
	gui.vimWindow->Unlock();
    }
}


void
gui_mch_set_bg_color(
	guicolor_T  color)
{
    rgb_color rgb = GUI_TO_RGB(color);
    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->SetLowColor(rgb);
	gui.vimWindow->Unlock();
    }
}


    void
gui_mch_set_sp_color(guicolor_T	color)
{
    
}

void
gui_mch_draw_string(
	int	row,
	int	col,
	char_u	*s,
	int	len,
	int	flags)
{
    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->mchDrawString(row, col, s, len, flags);
	gui.vimWindow->Unlock();
    }
}

	guicolor_T
gui_mch_get_rgb_color(int r, int g, int b)
{
    return gui_get_rgb_color_cmn(r, g, b);
}



int
gui_mch_haskey(
	char_u	*name)
{
    int i;

    for (i = 0; special_keys[i].BeKeys != 0; i++)
	if (name[0] == special_keys[i].vim_code0 &&
		name[1] == special_keys[i].vim_code1)
	    return OK;
    return FAIL;
}

    void
gui_mch_beep()
{
    ::beep();
}

    void
gui_mch_flash(int msec)
{
    

    if (gui.vimWindow->Lock()) {
	BRect rect = gui.vimTextArea->Bounds();

	gui.vimTextArea->SetDrawingMode(B_OP_INVERT);
	gui.vimTextArea->FillRect(rect);
	gui.vimTextArea->Sync();
	snooze(msec * 1000);	 
	gui.vimTextArea->FillRect(rect);
	gui.vimTextArea->SetDrawingMode(B_OP_COPY);
	gui.vimTextArea->Flush();
	gui.vimWindow->Unlock();
    }
}


void
gui_mch_invert_rectangle(
	int	r,
	int	c,
	int	nr,
	int	nc)
{
    BRect rect;
    rect.left = FILL_X(c);
    rect.top = FILL_Y(r);
    rect.right = rect.left + nc * gui.char_width - PEN_WIDTH;
    rect.bottom = rect.top + nr * gui.char_height - PEN_WIDTH;

    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->SetDrawingMode(B_OP_INVERT);
	gui.vimTextArea->FillRect(rect);
	gui.vimTextArea->SetDrawingMode(B_OP_COPY);
	gui.vimWindow->Unlock();
    }
}


    void
gui_mch_iconify()
{
    if (gui.vimWindow->Lock()) {
	gui.vimWindow->Minimize(true);
	gui.vimWindow->Unlock();
    }
}

#if defined(FEAT_EVAL) || defined(PROTO)

    void
gui_mch_set_foreground(void)
{
    
}
#endif


void
gui_mch_settitle(
	char_u	*title,
	char_u	*icon)
{
    if (gui.vimWindow->Lock()) {
	gui.vimWindow->SetTitle((char *)title);
	gui.vimWindow->Unlock();
    }
}


    void
gui_mch_draw_hollow_cursor(guicolor_T color)
{
    gui_mch_set_fg_color(color);

    BRect r;
    r.left = FILL_X(gui.col);
    r.top = FILL_Y(gui.row);
    int cells = utf_off2cells(LineOffset[gui.row] + gui.col, 100); 
    if (cells>=4) cells = 1;
    r.right = r.left + cells*gui.char_width - PEN_WIDTH;
    r.bottom = r.top + gui.char_height - PEN_WIDTH;

    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->StrokeRect(r);
	gui.vimWindow->Unlock();
	
    }
}


void
gui_mch_draw_part_cursor(
	int	w,
	int	h,
	guicolor_T  color)
{
    gui_mch_set_fg_color(color);

    BRect r;
    r.left =
#ifdef FEAT_RIGHTLEFT
	
	CURSOR_BAR_RIGHT ? FILL_X(gui.col + 1) - w :
#endif
	FILL_X(gui.col);
    r.right = r.left + w - PEN_WIDTH;
    r.bottom = FILL_Y(gui.row + 1) - PEN_WIDTH;
    r.top = r.bottom - h + PEN_WIDTH;

    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->FillRect(r);
	gui.vimWindow->Unlock();
	
    }
}


    void
gui_mch_update()
{
    gui_mch_flush();
    while (port_count(gui.vdcmp) > 0 &&
	    !vim_is_input_buf_full() &&
	    gui_haiku_process_event(0) >= B_OK)
	 ;
}


int
gui_mch_wait_for_chars(
	int	wtime)
{
    int		focus;
    bigtime_t	until, timeout;
    status_t	st;

    if (wtime >= 0)
    {
	timeout = wtime * 1000;
	until = system_time() + timeout;
    }
    else
	timeout = B_INFINITE_TIMEOUT;

    focus = gui.in_focus;
    for (;;)
    {
	
	if (gui.in_focus != focus)
	{
	    if (gui.in_focus)
		gui_mch_start_blink();
	    else
		gui_mch_stop_blink(TRUE);
	    focus = gui.in_focus;
	}

	gui_mch_flush();

#ifdef MESSAGE_QUEUE
# ifdef FEAT_TIMERS
	did_add_timer = FALSE;
# endif
	parse_queued_messages();
# ifdef FEAT_TIMERS
	if (did_add_timer)
	    
	    break;
# endif
# ifdef FEAT_JOB_CHANNEL
	if (has_any_channel())
	{
	    if (wtime < 0 || timeout > 20000)
		timeout = 20000;
	}
	else if (wtime < 0)
	    timeout = B_INFINITE_TIMEOUT;
# endif
#endif

	
	st = gui_haiku_process_event(timeout);

	if (input_available())
	    return OK;
	if (st < B_OK)		
	    return FAIL;

	
	if (wtime >= 0)
	{
	    timeout = until - system_time();
	    if (timeout < 0)
		break;
	}
    }
    return FAIL;

}




    void
gui_mch_flush()
{
    
    if (gui.vimWindow->Lock()) {
	gui.vimWindow->Flush();
	gui.vimWindow->Unlock();
    }
    return;
}


void
gui_mch_clear_block(
	int	row1,
	int	col1,
	int	row2,
	int	col2)
{
    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->mchClearBlock(row1, col1, row2, col2);
	gui.vimWindow->Unlock();
    }
}

    void
gui_mch_clear_all()
{
    if (gui.vimWindow->Lock()) {
	gui.vimTextArea->mchClearAll();
	gui.vimWindow->Unlock();
    }
}


void
gui_mch_delete_lines(
	int	row,
	int	num_lines)
{
    gui.vimTextArea->mchDeleteLines(row, num_lines);
}


void
gui_mch_insert_lines(
	int	row,
	int	num_lines)
{
    gui.vimTextArea->mchInsertLines(row, num_lines);
}

#if defined(FEAT_MENU) || defined(PROTO)


void
gui_mch_enable_menu(
	int	flag)
{
    if (gui.vimWindow->Lock())
    {
	BMenuBar *menubar = gui.vimForm->MenuBar();
	menubar->SetEnabled(flag);
	gui.vimWindow->Unlock();
    }
}

void
gui_mch_set_menu_pos(
	int	x,
	int	y,
	int	w,
	int	h)
{
    
}


void
gui_mch_add_menu(
	vimmenu_T   *menu,
	int	idx)
{
    vimmenu_T	*parent = menu->parent;

    
    if (menu_is_popup(menu->name) && parent == NULL) {
	BPopUpMenu* popUpMenu = new BPopUpMenu((const char*)menu->name, false, false);
	menu->submenu_id = popUpMenu;
	menu->id = NULL;
	return;
    }

    if (!menu_is_menubar(menu->name)
	    || (parent != NULL && parent->submenu_id == NULL))
	return;

    if (gui.vimWindow->Lock())
    {
	
	
	
	
	
	
	

	BMenu *tmp;

	if ( parent )
	    tmp = parent->submenu_id;
	else
	    tmp = gui.vimForm->MenuBar();
	
	

	if ( ! tmp->FindItem((const char *) menu->dname)) {

	    BMenu *bmenu = new BMenu((char *)menu->dname);

	    menu->submenu_id = bmenu;

	    
	    tmp->AddItem(bmenu);

	    
	    menu->id = tmp->FindItem((const char *) menu->dname);

	}
	gui.vimWindow->Unlock();
    }
}

    void
gui_mch_toggle_tearoffs(int enable)
{
    
}

    static BMessage *
MenuMessage(vimmenu_T *menu)
{
    BMessage *m = new BMessage('menu');
    m->AddPointer("VimMenu", (void *)menu);

    return m;
}


void
gui_mch_add_menu_item(
	vimmenu_T   *menu,
	int	idx)
{
    int	    mnemonic = 0;
    vimmenu_T	*parent = menu->parent;

    
    
    
    
    
    
    if (gui.vimWindow->Lock())
    {
#ifdef FEAT_TOOLBAR
	if (menu_is_toolbar(parent->name)) {
	    VimToolbar *toolbar = gui.vimForm->ToolBar();
	    if (toolbar != NULL) {
		toolbar->AddButton(idx, menu);
	    }
	} else
#endif

	if (parent->submenu_id != NULL || menu_is_popup(parent->name)) {
	    if (menu_is_separator(menu->name)) {
		BSeparatorItem *item = new BSeparatorItem();
		parent->submenu_id->AddItem(item);
		menu->id = item;
		menu->submenu_id = NULL;
	    }
	    else {
		BMenuItem *item = new BMenuItem((char *)menu->dname,
			MenuMessage(menu));
		item->SetTarget(gui.vimTextArea);
		item->SetTrigger((char) menu->mnemonic);
		parent->submenu_id->AddItem(item);
		menu->id = item;
		menu->submenu_id = NULL;
	    }
	}
	gui.vimWindow->Unlock();
    }
}


void
gui_mch_destroy_menu(
	vimmenu_T   *menu)
{
    if (gui.vimWindow->Lock())
    {
#ifdef FEAT_TOOLBAR
	if (menu->parent && menu_is_toolbar(menu->parent->name)) {
	    VimToolbar *toolbar = gui.vimForm->ToolBar();
	    if (toolbar != NULL) {
		toolbar->RemoveButton(menu);
	    }
	} else
#endif
	{
	    assert(menu->submenu_id == NULL || menu->submenu_id->CountItems() == 0);
	    
	    BMenu *bmenu = menu->id->Menu();
	    if (bmenu)
	    {
		bmenu->RemoveItem(menu->id);
		
		if (bmenu == gui.vimForm->MenuBar() && bmenu->CountItems() == 0)
		{
		    bmenu->ResizeTo(-MENUBAR_MARGIN, -MENUBAR_MARGIN);
		}
	    }
	    delete menu->id;
	    menu->id = NULL;
	    menu->submenu_id = NULL;

	    gui.menu_height = (int) gui.vimForm->MenuHeight();
	}
	gui.vimWindow->Unlock();
    }
}


void
gui_mch_menu_grey(
	vimmenu_T   *menu,
	int	grey)
{
#ifdef FEAT_TOOLBAR
    if (menu->parent && menu_is_toolbar(menu->parent->name)) {
	if (gui.vimWindow->Lock()) {
	    VimToolbar *toolbar = gui.vimForm->ToolBar();
	    if (toolbar != NULL) {
		toolbar->GrayButton(menu, grey);
	    }
	    gui.vimWindow->Unlock();
	}
    } else
#endif
    if (menu->id != NULL)
	menu->id->SetEnabled(!grey);
}


void
gui_mch_menu_hidden(
	vimmenu_T   *menu,
	int	hidden)
{
    if (menu->id != NULL)
	menu->id->SetEnabled(!hidden);
}


    void
gui_mch_draw_menubar()
{
    
}

    void
gui_mch_show_popupmenu(vimmenu_T *menu)
{
    if (!menu_is_popup(menu->name) || menu->submenu_id == NULL)
	return;

    BPopUpMenu* popupMenu = dynamic_cast<BPopUpMenu*>(menu->submenu_id);
    if (popupMenu == NULL)
	return;

    BPoint point;
    if (gui.vimWindow->Lock()) {
	uint32 buttons = 0;
	gui.vimTextArea->GetMouse(&point, &buttons);
	gui.vimTextArea->ConvertToScreen(&point);
	gui.vimWindow->Unlock();
    }
    popupMenu->Go(point, true);
}

#endif 



#ifdef FEAT_CLIPBOARD

char textplain[] = "text/plain";
char vimselectiontype[] = "application/x-vnd.Rhialto-Vim-selectiontype";


    void
clip_mch_request_selection(Clipboard_T *cbd)
{
    if (be_clipboard->Lock())
    {
	BMessage *m = be_clipboard->Data();
	

	char_u *string = NULL;
	ssize_t stringlen = -1;

	if (m->FindData(textplain, B_MIME_TYPE,
		    (const void **)&string, &stringlen) == B_OK
		|| m->FindString("text", (const char **)&string) == B_OK)
	{
	    if (stringlen == -1)
		stringlen = STRLEN(string);

	    int type;
	    char *seltype;
	    ssize_t seltypelen;

	    
	    if (m->FindData(vimselectiontype, B_MIME_TYPE,
			(const void **)&seltype, &seltypelen) == B_OK)
	    {
		switch (*seltype)
		{
		    default:
		    case 'L':	type = MLINE;	break;
		    case 'C':	type = MCHAR;	break;
#ifdef FEAT_VISUAL
		    case 'B':	type = MBLOCK;	break;
#endif
		}
	    }
	    else
	    {
		
		type = memchr(string, stringlen, '\n') ? MLINE : MCHAR;
	    }
	    clip_yank_selection(type, string, (long)stringlen, cbd);
	}
	be_clipboard->Unlock();
    }
}

    void
clip_mch_lose_selection(Clipboard_T *cbd)
{
    
}


    int
clip_mch_own_selection(Clipboard_T *cbd)
{
    
    return FAIL;
}


    void
clip_mch_set_selection(Clipboard_T *cbd)
{
    if (be_clipboard->Lock())
    {
	be_clipboard->Clear();
	BMessage *m = be_clipboard->Data();
	assert(m);

	
	cbd->owned = TRUE;
	clip_get_selection(cbd);
	cbd->owned = FALSE;

	char_u	*str = NULL;
	long_u	count;
	int type;

	type = clip_convert_selection(&str, &count, cbd);

	if (type < 0)
	    return;

	m->AddData(textplain, B_MIME_TYPE, (void *)str, count);

	
	char	vtype;
	switch (type)
	{
	    default:
	    case MLINE:    vtype = 'L';    break;
	    case MCHAR:    vtype = 'C';    break;
#ifdef FEAT_VISUAL
	    case MBLOCK:   vtype = 'B';    break;
#endif
	}
	m->AddData(vimselectiontype, B_MIME_TYPE, (void *)&vtype, 1);

	vim_free(str);

	be_clipboard->Commit();
	be_clipboard->Unlock();
    }
}

#endif	

#ifdef FEAT_BROWSE

char_u *
gui_mch_browse(
	int saving,
	char_u *title,
	char_u *dflt,
	char_u *ext,
	char_u *initdir,
	char_u *filter)
{
    gui.vimApp->fFilePanel = new BFilePanel((saving == TRUE) ? B_SAVE_PANEL : B_OPEN_PANEL,
	    NULL, NULL, 0, false,
	    new BMessage((saving == TRUE) ? 'save' : 'open'), NULL, true);

    gui.vimApp->fBrowsedPath.Unset();

    gui.vimApp->fFilePanel->Window()->SetTitle((char*)title);
    gui.vimApp->fFilePanel->SetPanelDirectory((const char*)initdir);

    gui.vimApp->fFilePanel->Show();

    gui.vimApp->fFilePanelSem = create_sem(0, "FilePanelSem");

    while (acquire_sem(gui.vimApp->fFilePanelSem) == B_INTERRUPTED);

    char_u *fileName = NULL;
    status_t result = gui.vimApp->fBrowsedPath.InitCheck();
    if (result == B_OK) {
	fileName = vim_strsave((char_u*)gui.vimApp->fBrowsedPath.Path());
    } else
	if (result != B_NO_INIT) {
	    fprintf(stderr, "gui_mch_browse: BPath error: %#08x (%s)\n",
		    result, strerror(result));
	}

    delete gui.vimApp->fFilePanel;
    gui.vimApp->fFilePanel = NULL;

    return fileName;
}
#endif 


#if defined(FEAT_GUI_DIALOG)



int
gui_mch_dialog(
	int	 type,
	char_u	*title,
	char_u	*message,
	char_u	*buttons,
	int	 dfltbutton,
	char_u	*textfield,
	int ex_cmd)
{
    VimDialog *dialog = new VimDialog(type, (char*)title, (char*)message,
	    (char*)buttons, dfltbutton, (char*)textfield, ex_cmd);
    return dialog->Go();
}

#endif 



    guicolor_T
gui_mch_get_rgb(guicolor_T pixel)
{
    rgb_color rgb = GUI_TO_RGB(pixel);

    return ((rgb.red & 0xff) << 16) + ((rgb.green & 0xff) << 8)
	+ (rgb.blue & 0xff);
}

    void
gui_mch_setmouse(int x, int y)
{
    TRACE();
    
}

#ifdef FEAT_MBYTE_IME
    void
im_set_position(int row, int col)
{
    if (gui.vimWindow->Lock())
    {
	gui.vimTextArea->DrawIMString();
	gui.vimWindow->Unlock();
    }
    return;
}
#endif

    void
gui_mch_show_toolbar(int showit)
{
    VimToolbar *toolbar = gui.vimForm->ToolBar();
    gui.toolbar_height = (toolbar && showit) ? toolbar->ToolbarHeight() : 0.;
}

    void
gui_mch_set_toolbar_pos(int x, int y, int w, int h)
{
    VimToolbar *toolbar = gui.vimForm->ToolBar();
    if (toolbar != NULL) {
	if (gui.vimWindow->Lock()) {
	    toolbar->MoveTo(x, y);
	    toolbar->ResizeTo(w - 1, h - 1);
	    gui.vimWindow->Unlock();
	}
    }
}

#if defined(FEAT_GUI_TABLINE) || defined(PROTO)


    void
gui_mch_show_tabline(int showit)
{
    VimTabLine *tabLine = gui.vimForm->TabLine();

    if (tabLine == NULL)
	return;

    if (!showit != !gui.vimForm->IsShowingTabLine()) {
	gui.vimForm->SetShowingTabLine(showit != 0);
	gui.tabline_height = gui.vimForm->TablineHeight();
    }
}

    void
gui_mch_set_tabline_pos(int x, int y, int w, int h)
{
    VimTabLine *tabLine = gui.vimForm->TabLine();
    if (tabLine != NULL) {
	if (gui.vimWindow->Lock()) {
	    tabLine->MoveTo(x, y);
	    tabLine->ResizeTo(w - 1, h - 1);
	    gui.vimWindow->Unlock();
	}
    }
}


    int
gui_mch_showing_tabline()
{
    VimTabLine *tabLine = gui.vimForm->TabLine();
    return tabLine != NULL && gui.vimForm->IsShowingTabLine();
}


    void
gui_mch_update_tabline()
{
    tabpage_T	*tp;
    int	    nr = 0;
    int	    curtabidx = 0;

    VimTabLine *tabLine = gui.vimForm->TabLine();

    if (tabLine == NULL)
	return;

    gui.vimWindow->Lock();

    
    for (tp = first_tabpage; tp != NULL; tp = tp->tp_next, ++nr) {
	if (tp == curtab)
	    curtabidx = nr;

	BTab* tab = tabLine->TabAt(nr);

	if (tab == NULL) {
	    tab = new VimTabLine::VimTab();
	    tabLine->AddTab(NULL, tab);
	}

	get_tabline_label(tp, FALSE);
	tab->SetLabel((const char*)NameBuff);
	tabLine->Invalidate();
    }

    
    while (nr < tabLine->CountTabs())
	tabLine->RemoveTab(nr);

    if (tabLine->Selection() != curtabidx)
	tabLine->Select(curtabidx);

    gui.vimWindow->Unlock();
}


    void
gui_mch_set_curtab(int nr)
{
    VimTabLine *tabLine = gui.vimForm->TabLine();
    if (tabLine == NULL)
	return;

    gui.vimWindow->Lock();

    if (tabLine->Selection() != nr -1)
	tabLine->Select(nr -1);

    gui.vimWindow->Unlock();
}

#endif 
