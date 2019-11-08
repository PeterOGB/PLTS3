// Vesion 3: Add Serial / Network comms selection

// TODO  Check for leaking GByteArray from convertToTelecode calls
#define G_LOG_USE_STRUCTURED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <gtk/gtk.h>


/* For network sockets */
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>

#include "Logging.h"






// Macro for geting widgets from the builder
#define GETWIDGET(name,type) name=type(gtk_builder_get_object_checked (builder,#name))

// Paths to directories. 
//static GString *sharedPath = NULL;
static GString *configPath = NULL;

// Widgets etc
static GtkComboBox *connectionCombobox;
GtkWidget *connectionWindow;
GtkAdjustment *ipAdjustments[4];
GtkWidget *mainWindow;

GtkWidget *makingConnectionDialog;
GtkWidget *reconnectButton;
gboolean reconnectButtonState = FALSE;

GtkRecentManager *recentManager;
GtkWidget *openRecentFileChooserDialog;
GtkWidget *loadFileChooserDialog;
GtkWidget *fileDownloadButton;
GtkWidget *fileDownloadFrameLabel;
GtkWidget *fileTooBigDialog;
GtkWidget *notTelecodeDialog;
GtkWidget *chooseFormatDialog;
GtkWidget *readerEchoButton;
GtkWidget *editorDownloadButton;
GtkWidget *downloadProgressBar;
GtkWidget *useLocalHost;


GtkWidget *tapeImageDrawingArea;

gboolean   fileDownloadWasSensitive = FALSE;
gboolean editorDownloadWasSensitive = FALSE;



/**************************** TELEPRINTER ************************/

GByteArray *punchingBuffer = NULL;
guchar runouts[16] = {[0 ... 15] = 0};

gboolean printing = TRUE;
gboolean punching = FALSE;

gboolean ELLIOTTcode = TRUE;
gboolean MURRYcode = FALSE;


GtkTextView *teleprinterTextView;
GtkTextBuffer *teleprinterTextBuffer;
GtkWidget *windUpFromStartButton;
GtkWidget *windUpFromEndButton;
GtkWidget *discardTapeButton;
GtkWidget *saveFileChooserDialog;
GtkWidget *punchingToTapeButton;


/*--------------------------- READER ----------------------------*/

GtkTextView *readerTextView;
GtkTextBuffer *readerTextBuffer;
gsize fileDownloadLength;
gsize fileDownloaded;
gchar *fileDownloadBuffer;
char *readerFileName = NULL;
gsize *downloadLengthp;
gsize *downloadedp;
gchar *downloadBuffer;
gboolean readerOnline = FALSE;
int handPosition = 0;


/*----------------------------- Editor -----------------------------*/
gsize editorDownloadLength;
gsize editorDownloaded;
gchar *editorDownloadBuffer;
GtkTextBuffer *editorTextBuffer;
GtkWidget *editorSaveButton;
GtkWidget *editorFrameLabel;
GtkWidget *editBinaryDialog;
char *editorFileName = NULL;

/* These tables use strings rather than single characters inorder to easily
   handle the two byte utf-8 code used for £ .
*/
// Elliott telecode
static const gchar  *convert2[] = {NULL,"1","2","*","4","$","=","7","8","'",
			     ",","+",":","-",".","%","0","(",")","3",
			     "?","5","6","/","@","9","£",NULL,NULL,
			     NULL,NULL,NULL,
			     NULL,"A","B","C","D","E","F","G","H", "I"
			     ,"J","K","L","M","N","O","P","Q","R","S",
			     "T","U","V","W","X","Y","Z",NULL," ",
			     "\r","\n",NULL};

// Murry Code
static const  gchar *convert3[] = {NULL,"3",NULL,"-"," ","'","8","7",NULL,NULL,"4",NULL,
			      ",","!",":","(","5","+",")","2","$","6","0","1","9","?","&",
			      NULL,".","/","=",NULL,
			      NULL,"E",NULL,"A"," ","S","I","U",NULL,"D","R","J","N",
			      "F","C","K","T","Z","L","W","H","Y","P","Q","O","B","G",
			      NULL,"M","X","V",NULL};




static GIOChannel *E803_channel = NULL;
static guint RxWatchId=0,TxWatchId=0,ErWatchId=0;


/******************* Prototypes for GTK event handlers **************************/

static GObject *gtk_builder_get_object_checked(GtkBuilder *builder,const gchar *name);
gboolean on_quitButton_clicked(__attribute__((unused)) GtkButton *widget,
			       __attribute__((unused)) gpointer data);
gboolean on_serialConnectButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused)) gpointer data);
gboolean on_networkConnectButton_clicked(__attribute__((unused)) GtkButton *button,
					 __attribute__((unused)) gpointer data);
gboolean on_noConnectionButton_clicked(__attribute__((unused)) GtkButton *widget,
				       __attribute__((unused)) gpointer data);
gboolean on_mainQuitButton_clicked(__attribute__((unused)) GtkButton *widget,
			       __attribute__((unused)) gpointer data);
gboolean on_reconnectButton_clicked(__attribute__((unused)) GtkButton *widget,
				    __attribute__((unused)) gpointer data);
gboolean
on_fileDownloadSetFileButton_clicked(__attribute__((unused)) GtkButton *button,
				     __attribute__((unused))gpointer data);
gboolean
on_fileDownloadButton_clicked(__attribute__((unused)) GtkButton *button,
			      __attribute__((unused)) gpointer data);


static gboolean
E803_messageHandler(GIOChannel *source,
		    __attribute__((unused)) GIOCondition condition,
		    __attribute__((unused)) gpointer data);



gboolean
on_printToScreenButton_toggled( GtkToggleButton *button,
				__attribute__((unused))gpointer data);


gboolean
on_punchingToTapeButton_toggled(GtkToggleButton *button,
				__attribute__((unused)) gpointer data);

gboolean
on_windUpFromEndButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused))	gpointer data);

gboolean
on_windUpFromStartButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data);

gboolean
on_discardTapeButton_clicked(__attribute__((unused)) GtkButton *button,
			     __attribute__((unused))	gpointer data);
gboolean
on_clearTeleprinterTextButton_clicked(__attribute__((unused)) GtkButton *button,
				      __attribute__((unused))	gpointer data);

gboolean
on_readerEchoButton_toggled(GtkWidget *button,
			    __attribute__((unused))gpointer user_data);

gboolean
on_fileDownloadChooseRecentFileButton_clicked(__attribute__((unused)) GtkButton *button,
					      __attribute__((unused))gpointer data);

gboolean on_editorTextView_key_press_event(__attribute__((unused))GtkWidget *widget,
					   GdkEventKey *event);

gboolean
on_editorNewButton_clicked(__attribute__((unused)) GtkButton *button,
			   __attribute__((unused))gpointer data);

gboolean
on_editorOldButton_clicked(__attribute__((unused)) GtkButton *widget,
			   __attribute__((unused))gpointer data);

gboolean
on_editorDownloadButton_clicked(__attribute__((unused)) GtkButton *button,
				__attribute__((unused)) gpointer data);

gboolean
on_editorDownloadFromCursorButton_clicked(__attribute__((unused)) GtkButton *button,
					  __attribute__((unused)) gpointer data);

gboolean
on_editorSetFileButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused))	gpointer data);

gboolean
on_editorSaveButton_clicked(__attribute__((unused)) GtkButton *button,
			    __attribute__((unused))gpointer data);

gboolean
on_readerOnlineCheckButton_toggled(__attribute__((unused)) GtkButton *button,
				   __attribute__((unused)) gpointer data);
gboolean
on_readerTextView_key_press_eventt(__attribute__((unused))GtkWidget *widget,
				   GdkEventKey *event);

gboolean
on_readerTextView_key_press_event(__attribute__((unused))GtkWidget *widget,
				  GdkEventKey *event);

gboolean
on_useMurryCodeButton_toggled(__attribute__((unused)) GtkButton *button,
			      __attribute__((unused)) gpointer data);

gboolean
on_clearReaderTextButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data);

gboolean
on_setDefaultAddressButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data);


static gboolean
notTelecode(GdkEventKey *event,gboolean Online);

/* Widget Event Handlers */
/* Connection window */

gboolean
on_quitButton_clicked(__attribute__((unused)) GtkButton *widget,
		      __attribute__((unused)) gpointer data)
{
    gtk_main_quit();
    return GDK_EVENT_STOP ;
}
gboolean
on_noConnectionButton_clicked(__attribute__((unused)) GtkButton *widget,
		      __attribute__((unused)) gpointer data)
{

    if(!gtk_widget_get_visible(mainWindow))
    {
	//title = g_string_new(NULL);
	//g_string_printf(title,"Connected to %s",address->str);
	//gtk_window_set_title(GTK_WINDOW(mainWindow),title->str);
	gtk_widget_show(mainWindow);
	    
    }
    gtk_widget_hide(connectionWindow);
    return GDK_EVENT_PROPAGATE ;
}

gboolean
on_setDefaultAddressButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data)
{
    GString *address;
    	GString *configFileName;
	GIOChannel *file;
	GError *error = NULL;





    address = g_string_new(NULL);
    g_string_printf(address,"%d.%d.%d.%d\n",
			(int)gtk_adjustment_get_value(ipAdjustments[0]),
			(int)gtk_adjustment_get_value(ipAdjustments[1]),
			(int)gtk_adjustment_get_value(ipAdjustments[2]),
			(int)gtk_adjustment_get_value(ipAdjustments[3]));


    g_info("Adddress set to (%s)\n",address->str);
    configFileName = g_string_new(configPath->str);
    g_string_append(configFileName,"DefaultIP");

    if((file = g_io_channel_new_file(configFileName->str,"w",&error)) == NULL)
    {
	g_warning("failed to open file %s due to %s\n",configFileName->str,error->message);
	
    }
    else
    {
	g_io_channel_write_chars(file,address->str,-1,NULL,NULL);

	g_io_channel_shutdown(file,TRUE,NULL);
	g_io_channel_unref (file);


    }
    
    return GDK_EVENT_PROPAGATE ;
}
    


gboolean
on_serialConnectButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused)) gpointer data)
{
    gint active;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchararray name;
    GString *title;
    
    struct termios newtio;
    int serial_fd;

    active = gtk_combo_box_get_active(connectionCombobox);
    model = gtk_combo_box_get_model(connectionCombobox);

   
    gtk_combo_box_get_active_iter(connectionCombobox,&iter);

    gtk_tree_model_get(model,&iter,0,&name,-1);
       
    printf("%s called %d  %s\n",__FUNCTION__,active,name);


    serial_fd = open(name,O_RDWR|O_NONBLOCK);

    tcgetattr(serial_fd,&newtio);
    cfsetospeed(&newtio,B57600);
    newtio.c_cc[VMIN] = 0;
    newtio.c_cc[VTIME] = 0;
    cfmakeraw(&newtio);
    newtio.c_cflag &= ~CRTSCTS;
    tcsetattr(serial_fd,TCSANOW,&newtio);

    E803_channel = g_io_channel_unix_new(serial_fd);
  
    RxWatchId = g_io_add_watch(E803_channel,G_IO_IN ,E803_messageHandler,NULL);

    g_io_channel_set_encoding(E803_channel,NULL,NULL);
    // Channel needs to be unbuffered otherwise interleaved reads and writes lead to
    // "Illegal seek" errors !
    g_io_channel_set_buffered(E803_channel,FALSE);

    gtk_widget_hide(connectionWindow);
    if(!gtk_widget_get_visible(mainWindow))
    {
	gtk_widget_show(mainWindow);
    }
    title = g_string_new(NULL);
    g_string_printf(title,"Connected to PLTS via %s",name);
    gtk_window_set_title(GTK_WINDOW(mainWindow),title->str);
    g_string_free(title,TRUE);
				 
    gtk_button_set_label(GTK_BUTTON(reconnectButton),"gtk-disconnect");
    reconnectButtonState = TRUE;
     
    return GDK_EVENT_PROPAGATE ;
}


/* Main window event handlers */

gboolean on_mainQuitButton_clicked(__attribute__((unused)) GtkButton *widget,
			       __attribute__((unused)) gpointer data)
{
    gtk_main_quit();
    return TRUE;
}

gboolean on_reconnectButton_clicked(__attribute__((unused)) GtkButton *widget,
			       __attribute__((unused)) gpointer data)
{
    if(reconnectButtonState)
    {
	g_source_remove(RxWatchId);
	if(ErWatchId>0)	g_source_remove(ErWatchId);
	g_io_channel_shutdown(E803_channel,FALSE,NULL);
	g_io_channel_unref(E803_channel);
	E803_channel = NULL;
	gtk_button_set_label(GTK_BUTTON(reconnectButton),"gtk-connect");
	reconnectButtonState = FALSE;
	gtk_window_set_title(GTK_WINDOW(mainWindow),"Disconnected");
    }
    else
    {
	gtk_widget_show (connectionWindow);
    }
    return GDK_EVENT_PROPAGATE ;
}

static gboolean
isTelecodeTape(gchar *buf,gsize len)
{
    gsize n;
    gchar *cp;

    cp = buf; //readerDownloadBuffer;
    n = len; //readerDownloadLength;

    // Check for any chars with -XX---- bits set as telecode
    // files will not have any ones there.
    while(n--)
	if( (*cp++ & 0x60) != 0x00)
	    return FALSE;

    return TRUE;
}

static gboolean
isBinaryTape(gchar *buf,gsize len)
{
    gsize count,index;
    gint crCount,lfCount,crlfCount;
    gint diff1,diff2,diff3;
    gchar this,prev;

    printf("%s %p %ld\n",__FUNCTION__,buf,len);
    
    count = len;
    index = 0;
    crCount = lfCount = crlfCount = 0;

    this = prev = 0x00;
    while(count-- > 1)
    {
	prev = this;
	this = buf[index] & 0x1F;
	
	//printf("%d %d %02X\n",count,index,this);
	// Check for CR/LF sequences
	if(this == 0x1D) crCount+= 1;
	if(this == 0x1E) lfCount+= 1;

	if( (this == 0x1E) &&
	    (prev == 0x1D) ) crlfCount+= 1;
	index += 1;
    }

    printf("isBinaryTape: %d %d %d\n",crCount,lfCount,crlfCount);


    diff1 = abs(crCount - lfCount);
    diff2 = abs(crCount - crlfCount);
    diff3 = abs(lfCount - crlfCount);
    
    printf("isBinaryTape: %d %d %d\n",diff1,diff2,diff3);

    if(diff1 < 5)
	return FALSE;
    else
	return TRUE;

}


/*************** READER ********************/

gboolean
on_fileDownloadSetFileButton_clicked(__attribute__((unused)) GtkButton *button,
				     __attribute__((unused))gpointer data)
{
    //GtkWidget *dialog;
    gint res;
    GError *error = NULL;

    gtk_widget_set_sensitive(fileDownloadButton,FALSE);
    
    
    res = gtk_dialog_run (GTK_DIALOG (loadFileChooserDialog));
    gtk_widget_hide(loadFileChooserDialog);

    if (res == GTK_RESPONSE_OK)
    {
	GString *title;
	
	if(readerFileName != NULL)
	    g_free (readerFileName);
	readerFileName = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (loadFileChooserDialog));

	//gtk_recent_manager_add_item(recentManager,readerFileName);
	
	title = g_string_new("File Download: ");
	g_string_append(title,readerFileName);
	gtk_label_set_text(GTK_LABEL(fileDownloadFrameLabel),title->str);
	g_string_free(title,TRUE);
    }
    else
    {
	if(fileDownloadBuffer != NULL)
	    gtk_widget_set_sensitive(fileDownloadButton,TRUE);
	return GDK_EVENT_PROPAGATE ;
    }
    
    if(fileDownloadBuffer != NULL)
	g_free(fileDownloadBuffer);
    
    g_file_get_contents (readerFileName,&fileDownloadBuffer,&fileDownloadLength,&error);

    if(fileDownloadLength > 65536)
    {
	res = gtk_dialog_run (GTK_DIALOG (fileTooBigDialog));
	gtk_widget_hide(fileTooBigDialog);
	//gtk_widget_set_sensitive(fileDownloadButton,FALSE);
	return GDK_EVENT_PROPAGATE ;
    }
	    
    if(!isTelecodeTape(fileDownloadBuffer,fileDownloadLength))
    {
    	res = gtk_dialog_run (GTK_DIALOG (notTelecodeDialog));
	gtk_widget_hide(notTelecodeDialog);

	g_free(fileDownloadBuffer);
	fileDownloadBuffer = NULL;
	
	return GDK_EVENT_PROPAGATE ;
    }
	
    printf("filename=%s\n",readerFileName);
    gtk_widget_set_sensitive(fileDownloadButton,TRUE);

    {
	gsize n;
	char *cp;
	cp = fileDownloadBuffer;
	n = fileDownloadLength;
	while(n--) *cp++ |= '\x80';
    }

    // Update the tape image
    gtk_widget_queue_draw(tapeImageDrawingArea);

    return GDK_EVENT_PROPAGATE ;
}

gboolean
on_readerEchoButton_toggled(GtkWidget *button,
			    __attribute__((unused))gpointer user_data)
{
  gchar value[1];
  
  gboolean set;
  gsize written;
  GError *error = NULL;

  printf("%s called\n",__FUNCTION__);

  set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

  if(set) 
     value[0] = '\x84';
  else
    value[0] = '\x85';

  g_io_channel_write_chars(E803_channel,value,1,&written,&error);
  g_io_channel_flush(E803_channel,NULL);

  return GDK_EVENT_PROPAGATE ;
}





gboolean
on_fileDownloadButton_clicked(__attribute__((unused)) GtkButton *button,
				__attribute__((unused)) gpointer data)
{
    //GtkWidget *dialog;
    //gint res;
    
    gchar value[3];
    //struct stat statBuf;
    //int fd;
    GError *error = NULL;
    gsize written;
    gsize n;

    printf("%s %s %d\n",__FUNCTION__,readerFileName,handPosition);


    //g_free(readerDownloadBuffer);
	  
    //g_file_get_contents (readerFileName,&readerDownloadBuffer,&readerDownloadLength,&error);

    // Check if file is binary tape image file

    if(isBinaryTape(fileDownloadBuffer,fileDownloadLength))
    {
	printf("BINARY \n");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(readerEchoButton),FALSE);
    }

    
    for(n=0;n<fileDownloadLength;n++) fileDownloadBuffer[n] &= 0x1F;

    value[0] = '\x80';
    value[1] = '\x00';
    value[2] = '\x00';
    
    g_io_channel_write_chars(E803_channel,value,3,&written,&error);
    g_io_channel_flush(E803_channel,NULL);

    // The cast to intmax_t makes this work as expected on 
    // the 32 bit ARM CPU inthe Pi. 
    printf("file size = %jd \n",(__intmax_t)fileDownloadLength);
    fileDownloaded = 0;

    /*
    downloadLengthp = &fileDownloadLength;
    downloadedp = &fileDownloaded;
    downloadBuffer = fileDownloadBuffer;
    */

    fileDownloadLength -= (gsize) (handPosition/8);
    downloadLengthp = &fileDownloadLength;
    downloadedp = &fileDownloaded;
    downloadBuffer = &fileDownloadBuffer[handPosition/8];

    // Save button states
    fileDownloadWasSensitive   = gtk_widget_get_sensitive(fileDownloadButton);
    editorDownloadWasSensitive = gtk_widget_get_sensitive(editorDownloadButton);
    
    gtk_widget_set_sensitive(fileDownloadButton,FALSE);
    gtk_widget_set_sensitive(editorDownloadButton,FALSE);
    return GDK_EVENT_PROPAGATE ;
}


gboolean
on_fileDownloadChooseRecentFileButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused))gpointer data)
{
    gint res;
    GError *error = NULL;

    res = gtk_dialog_run (GTK_DIALOG (openRecentFileChooserDialog));
    gtk_widget_hide(openRecentFileChooserDialog);

    if (res == GTK_RESPONSE_OK)
    {
	GString *title;
	GtkRecentInfo *info;
	
	if(readerFileName != NULL)
	    g_free (readerFileName);

	info =  gtk_recent_chooser_get_current_item (GTK_RECENT_CHOOSER(openRecentFileChooserDialog));
	readerFileName = gtk_recent_info_get_uri_display(info);

	
	title = g_string_new("File Download: ");
	g_string_append(title,readerFileName);
	gtk_label_set_text(GTK_LABEL(fileDownloadFrameLabel),title->str);
	g_string_free(title,TRUE);
	gtk_recent_info_unref (info);
    }
    else
    {
	if(fileDownloadBuffer != NULL)
	    gtk_widget_set_sensitive(fileDownloadButton,TRUE);
	return GDK_EVENT_STOP;
    }
    
    if(fileDownloadBuffer != NULL)
	g_free(fileDownloadBuffer);
    
    g_file_get_contents (readerFileName,&fileDownloadBuffer,&fileDownloadLength,&error);

    if(fileDownloadLength > 65536)
    {
	res = gtk_dialog_run (GTK_DIALOG (fileTooBigDialog));
	gtk_widget_hide(fileTooBigDialog);
	//gtk_widget_set_sensitive(fileDownloadButton,FALSE);
	return GDK_EVENT_STOP ;
    }
	    
    if(!isTelecodeTape(fileDownloadBuffer,fileDownloadLength))
    {
    	res = gtk_dialog_run (GTK_DIALOG (notTelecodeDialog));
	gtk_widget_hide(notTelecodeDialog);

	g_free(fileDownloadBuffer);
	fileDownloadBuffer = NULL;
	
	return GDK_EVENT_STOP ;
    }
	
    printf("filename=%s\n",readerFileName);
    gtk_widget_set_sensitive(fileDownloadButton,TRUE);
    return GDK_EVENT_PROPAGATE ;
    
}

static gboolean goingOnline = FALSE;

gboolean
on_readerOnlineCheckButton_toggled(__attribute__((unused)) GtkButton *button,
				   __attribute__((unused)) gpointer data)
{
    gchar value[1];
    //gboolean set;
    gsize written;
    GError *error = NULL;
      
    readerOnline = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) ;
    printf("%s %s\n",__FUNCTION__,readerOnline ? "TRUE" : "FALSE");

    if(readerOnline)
    {
	value[0] = '\x88';
	goingOnline = TRUE;
    }
    else
	value[0] = '\x89';

    g_io_channel_write_chars(E803_channel,value,1,&written,&error);
    if(error != NULL) printf("ERROR !=NULL %s %d\n",__FUNCTION__,__LINE__);
    g_io_channel_flush(E803_channel,NULL);

    return GDK_EVENT_PROPAGATE;
}

guint keyToTelecode[32];
guint keyToMurryCode[64];

gboolean
on_readerTextView_key_press_event(__attribute__((unused))GtkWidget *widget,
				   GdkEventKey *event)
{
    gboolean ignore;
       gchar value[2];
    guint telecode,n;
 
    gsize written;
    GError *error = NULL;
    static gboolean forceShift = FALSE;
    
    if(readerOnline)
    {
	ignore = notTelecode(event,TRUE);
	printf("%s %s\n",__FUNCTION__,ignore ? "TRUE" : "FALSE");
	if(ignore) return GDK_EVENT_STOP;

	if(goingOnline)
	{
	    forceShift = TRUE;
	    goingOnline = FALSE;
	}

	if(ELLIOTTcode)
	{
	    if((event->keyval >= GDK_KEY_A) && (event->keyval <= GDK_KEY_Z))
	    {
		telecode = 1 + event->keyval - GDK_KEY_A ;
		telecode += 32;
	    }
	    else if((event->keyval >= GDK_KEY_a) && (event->keyval <= GDK_KEY_z))
	    {
		telecode = 1+ event->keyval - GDK_KEY_a;
		telecode += 32;


	    }
	    else
	    {
		for(n=0;n<32;n++)
		    if(keyToTelecode[n] == event->keyval)
		    {
			//if(n == 29) n = 30;   // CR -> LF
			// Always send a FS or LS after a CR
			if(n == 29) forceShift = TRUE;
			telecode = n;
			if(n >= 27) telecode += 64;
		    }
	    
	    }
	}

	if(MURRYcode)
	{
	    for(n=0;n<64;n++)
	    {
		if(keyToMurryCode[n] == event->keyval)
		{
		    if(n == 2) forceShift = TRUE;
		    telecode = n;
		    if((n == 4)||(n == 8)||(n == 2)) telecode += 64;
		}

	    }
	
	}
	if(forceShift && (telecode < 64))
	{
	    /*
	    gchar shift;
	    shift = 0x5B + ((telecode >> 3) & 0x4);
	    value[0] = 0x8A;
	    value[1] = shift;
	    printf("%s telecode = 0x%2X\n",__FUNCTION__,telecode & 0xFF);

	    g_io_channel_write_chars(E803_channel,value,2,&written,&error);
	    if(error != NULL) printf("ERROR !=NULL %s %d\n",__FUNCTION__,__LINE__);
	    g_io_channel_flush(E803_channel,NULL);
	    */
	    telecode += 0x80;
	    forceShift = FALSE;
	}
	    
	    
       	value[0] = '\x8A';
	value[1] = (gchar) (telecode & 0xFF);
	printf("%s telecode = 0x%2X\n",__FUNCTION__,telecode & 0xFF);

	g_io_channel_write_chars(E803_channel,value,2,&written,&error);
	if(error != NULL) printf("ERROR !=NULL %s %d\n",__FUNCTION__,__LINE__);
	g_io_channel_flush(E803_channel,NULL);


	return GDK_EVENT_STOP;

	

    }
    else
    {
	return GDK_EVENT_STOP;
    }
}

gboolean
on_clearReaderTextButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data)
{
    GtkTextIter start,end;

    gtk_text_buffer_get_start_iter (readerTextBuffer,&start);
    gtk_text_buffer_get_end_iter (readerTextBuffer,&end);

    gtk_text_buffer_delete (readerTextBuffer,&start,&end);
    return GDK_EVENT_PROPAGATE;
}

/************************** TELEPRINTER *************************/



gboolean
on_printToScreenButton_toggled( GtkToggleButton *button,
				__attribute__((unused))gpointer data)
{
  gboolean set;



  set = gtk_toggle_button_get_active(button);
  if(set) 
     printing = TRUE;
  else
    printing = FALSE;

  printf("printing is %s\n",printing?"True":"False");

  return GDK_EVENT_PROPAGATE ;

}

gboolean
on_punchingToTapeButton_toggled(GtkToggleButton *button,
			     __attribute__((unused)) gpointer data)
{

  gboolean set;



  set = gtk_toggle_button_get_active(button);
  if(set)
  {
      punching = TRUE;
      if(punchingBuffer == NULL)
      {
	  punchingBuffer = g_byte_array_sized_new(1024);
	  g_byte_array_append(punchingBuffer,runouts,16);
	  gtk_widget_set_sensitive(windUpFromStartButton,TRUE);
	  gtk_widget_set_sensitive(windUpFromEndButton,TRUE);
	  gtk_widget_set_sensitive(discardTapeButton,TRUE);
      }
  }  
  else
      punching = FALSE;

  printf("punching is %s\n",punching?"True":"False");

  return GDK_EVENT_PROPAGATE ;
}

static gboolean
saveBuffer(GByteArray *buffer)
{
    gint res;
    
    GError *error = NULL;

    res = gtk_dialog_run (GTK_DIALOG (saveFileChooserDialog));

    gtk_widget_hide(saveFileChooserDialog);
    
    if (res == GTK_RESPONSE_OK)
    {
	gchar *punchFileName;
	punchFileName = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (saveFileChooserDialog));

	error = NULL;
	
	g_file_set_contents (punchFileName,(gchar *) buffer->data,buffer->len,&error);

	g_free(punchFileName);
	return TRUE;
    }

    return FALSE;
}



gboolean 
on_windUpFromEndButton_clicked(__attribute__((unused)) GtkButton *button,
			     __attribute__((unused))	gpointer data)
{

    g_byte_array_append(punchingBuffer,runouts,16);
    if(saveBuffer(punchingBuffer))
    {
	g_byte_array_free(punchingBuffer,TRUE);
	punchingBuffer = NULL;

	gtk_widget_set_sensitive(windUpFromStartButton,FALSE);
	gtk_widget_set_sensitive(windUpFromEndButton,FALSE);
	gtk_widget_set_sensitive(discardTapeButton,FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(punchingToTapeButton),FALSE);
    }
    return  GDK_EVENT_PROPAGATE;
    
}


static void
reverseBuffer(GByteArray *buffer)
{
    guint8 *data,tmp;
    guint fromStart,fromEnd; 

    data = buffer->data;
    // BUg fix 29/9/19   Needed -1
    fromEnd = buffer->len - 1;

    for(fromStart = 0; fromStart < fromEnd; fromStart ++, fromEnd --) {
	tmp = data[fromStart];
	data[fromStart] = data[fromEnd];
	data[fromEnd] = tmp;
    }
}


gboolean
on_windUpFromStartButton_clicked(__attribute__((unused)) GtkButton *button,
				 __attribute__((unused)) gpointer data)
{
    // Add runouts to the end that will become blank header
    g_byte_array_append(punchingBuffer,runouts,16);
    reverseBuffer(punchingBuffer);
    
    if(saveBuffer(punchingBuffer))
    {
	g_byte_array_free(punchingBuffer,TRUE);
	punchingBuffer = NULL;

	gtk_widget_set_sensitive(windUpFromStartButton,FALSE);
	gtk_widget_set_sensitive(windUpFromEndButton,FALSE);
	gtk_widget_set_sensitive(discardTapeButton,FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(punchingToTapeButton),FALSE);
    }
    else
    {
	// Restore if not saved
	reverseBuffer(punchingBuffer);
    }

    return  GDK_EVENT_PROPAGATE;

    
}

gboolean
on_discardTapeButton_clicked(__attribute__((unused)) GtkButton *button,
			     __attribute__((unused))	gpointer data)
{
    printf("%s called\n",__FUNCTION__);
    if(punchingBuffer != NULL)
	g_byte_array_set_size(punchingBuffer,16);  // Leave runouts in place
    return  GDK_EVENT_PROPAGATE;
}


gboolean
on_clearTeleprinterTextButton_clicked(__attribute__((unused)) GtkButton *button,
				      __attribute__((unused))	gpointer data)
{
    GtkTextIter start,end;

    gtk_text_buffer_get_start_iter (teleprinterTextBuffer,&start);
    gtk_text_buffer_get_end_iter (teleprinterTextBuffer,&end);

    gtk_text_buffer_delete (teleprinterTextBuffer,&start,&end);

    return  GDK_EVENT_PROPAGATE;
}

gboolean
on_useMurryCodeButton_toggled(__attribute__((unused)) GtkButton *button,
			      __attribute__((unused)) gpointer data)
{
    gboolean set;
      
    set = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) ;
    printf("%s %s\n",__FUNCTION__,set ? "TRUE" : "FALSE");

    if(set)
    {
	MURRYcode = TRUE;
	ELLIOTTcode = FALSE;
    }
    else
    {
	MURRYcode = FALSE;
	ELLIOTTcode = TRUE;
    }	
    return  GDK_EVENT_PROPAGATE;
}


/*********************************** EDITOR ********************************/

/* These tables use strings rather than single characters inorder to easily
   handle the two byte utf-8 code used for £ .
*/



/* GDK_KEY codes for the non-letter and non-digit characters in Elliott Telecode */
guint telecodeKeysElliott[16] = {GDK_KEY_asterisk,GDK_KEY_dollar,GDK_KEY_equal,GDK_KEY_apostrophe,
			GDK_KEY_comma,GDK_KEY_plus,GDK_KEY_colon,GDK_KEY_minus,
			GDK_KEY_period,GDK_KEY_percent,GDK_KEY_parenleft,
			GDK_KEY_parenright,GDK_KEY_question,GDK_KEY_slash,GDK_KEY_at,
			GDK_KEY_sterling};


/* GDK_KEY codes for the non-letter and non-digit characters in Murry code */
guint telecodeKeysMurry[14] = {GDK_KEY_minus,GDK_KEY_question,GDK_KEY_colon,
				 GDK_KEY_percent,GDK_KEY_at,GDK_KEY_sterling,
				 GDK_KEY_parenleft,GDK_KEY_parenright,GDK_KEY_period,
				 GDK_KEY_comma,GDK_KEY_apostrophe,GDK_KEY_equal,
				 GDK_KEY_slash,GDK_KEY_plus};



/* GDK_KEY codes for other allowed key presses */
guint cursorKeys[7] = {GDK_KEY_Return,GDK_KEY_BackSpace,GDK_KEY_Left,GDK_KEY_Up,
		       GDK_KEY_Right,GDK_KEY_Down,GDK_KEY_space };

guint keyToTelecode[32] = {0,GDK_KEY_1,GDK_KEY_2,GDK_KEY_asterisk,GDK_KEY_4,
			   GDK_KEY_dollar,GDK_KEY_equal,GDK_KEY_7,GDK_KEY_8,
			   GDK_KEY_apostrophe,GDK_KEY_comma,GDK_KEY_plus,
			   GDK_KEY_colon,GDK_KEY_minus,GDK_KEY_period,
			   GDK_KEY_percent,GDK_KEY_0,GDK_KEY_parenleft,
			   GDK_KEY_parenright,GDK_KEY_3,GDK_KEY_question,
			   GDK_KEY_5,GDK_KEY_6,GDK_KEY_slash,GDK_KEY_at,
			   GDK_KEY_9,GDK_KEY_sterling,0,GDK_KEY_space,
			   GDK_KEY_Return,GDK_KEY_BackSpace,0};

guint keyToMurryCode[64] = {0,GDK_KEY_5,GDK_KEY_Return,GDK_KEY_9,GDK_KEY_space,GDK_KEY_sterling,
			    GDK_KEY_comma,GDK_KEY_period,GDK_KEY_BackSpace,GDK_KEY_parenright,
			    GDK_KEY_4,GDK_KEY_at,GDK_KEY_8,GDK_KEY_0,GDK_KEY_colon,GDK_KEY_equal,
			    GDK_KEY_3,GDK_KEY_plus,0,GDK_KEY_question,GDK_KEY_apostrophe,
			    GDK_KEY_6,GDK_KEY_percent,GDK_KEY_slash,GDK_KEY_minus,GDK_KEY_2,0,
			    0,GDK_KEY_7,GDK_KEY_1,GDK_KEY_parenleft,0,

			    0,GDK_KEY_t,GDK_KEY_Return,GDK_KEY_o,GDK_KEY_space,GDK_KEY_h,GDK_KEY_n,
			    GDK_KEY_m,GDK_KEY_BackSpace,GDK_KEY_l,GDK_KEY_r,GDK_KEY_g,GDK_KEY_i,GDK_KEY_p,
			    GDK_KEY_c,GDK_KEY_v,GDK_KEY_e,GDK_KEY_z,GDK_KEY_d,GDK_KEY_b,GDK_KEY_s,GDK_KEY_y,
			    GDK_KEY_f,GDK_KEY_x,GDK_KEY_a,GDK_KEY_w,GDK_KEY_j,0,GDK_KEY_u,GDK_KEY_q,GDK_KEY_k,0};


static gboolean
notTelecode(GdkEventKey *event,gboolean Online)
{
    int n;
   // Allow A-Z
    if((event->keyval >= GDK_KEY_A) && (event->keyval <= GDK_KEY_Z))
	return FALSE;

    // Allow a-z 
    if((event->keyval >= GDK_KEY_a) && (event->keyval <= GDK_KEY_z))
    	return FALSE;

    // Allow 0-9
    if((event->keyval >= GDK_KEY_0) && (event->keyval <= GDK_KEY_9))
	return FALSE;


    if(Online)
    {
	if(ELLIOTTcode)
	{
	    // Allow other valid figureshift characters
	    for(n=0;n<16;n++)
		if(telecodeKeysElliott[n] == event->keyval)
		    return FALSE;
	}
	if(MURRYcode)
	{
	    // Allow other valid figureshift characters
	    for(n=0;n<13;n++)
		if(telecodeKeysMurry[n] == event->keyval)
		    return FALSE;

	}
	
	if(event->keyval == GDK_KEY_Return) return FALSE;
	if(event->keyval == GDK_KEY_space) return FALSE;
	if(event->keyval == GDK_KEY_BackSpace) return FALSE;
    }
    else
    {
	// Allow other valid figureshift characters
	for(n=0;n<16;n++)
	    if(telecodeKeysElliott[n] == event->keyval)
		return FALSE;
		
	// Allow control keys etc
	for(n=0;n<7;n++)
	    if(cursorKeys[n] == event->keyval)
		return FALSE;

	// Allow "#"   It's not telecode, but us used ignore the rest of
	// line (as a comment) when converting to telecode.

	if(event->keyval == GDK_KEY_numbersign) return FALSE;
	
    }
    // Ignore everything else !
    return TRUE;


}


guint8 shifts[5] = {0x1B,0x1C,0x1D,0x1E,0x1F};

static GByteArray *
convertToTelecode(gboolean fromCursorFlag)
{
    gboolean letters,figures,skipping,ignoreLF;
    GtkTextIter start,end;
    guchar *utf8text,*cp;    // Made uchar otherwise utf8 code didn't work
/*
/home/petero/ELLIOTT-803/803-PLTS-2/PLTS2.c:684:9: warning: comparison is always true due to limited range of data type [-Wtype-limits]
  if(*cp < 0x80)

 */    
    guchar utf8char[5];
    gint length,count,n;
    guint8 m;
    GByteArray *telecode;
    
    letters = figures = FALSE;


    if(fromCursorFlag)
    {
	GtkTextMark *tm;
	tm = gtk_text_buffer_get_insert (editorTextBuffer);

	gtk_text_buffer_get_iter_at_mark (editorTextBuffer,
                                  &start,
                                  tm);
	gtk_text_buffer_get_end_iter (editorTextBuffer,&end);
    }
    else
    {
	if(!gtk_text_buffer_get_selection_bounds (editorTextBuffer,&start,&end))
	{
	    gtk_text_buffer_get_start_iter (editorTextBuffer,&start);
	    gtk_text_buffer_get_end_iter (editorTextBuffer,&end);
	}
    }
    utf8text =  (guchar *) gtk_text_buffer_get_text (editorTextBuffer,&start,&end,FALSE);
    length = gtk_text_buffer_get_char_count (editorTextBuffer);
    // Start off with worstcase-ish guess at size
    telecode = g_byte_array_sized_new (2 * (guint)length);
    count = 0;

    // Add some runnouts so that saved telecode files can be imported into the
    // emulator and loaded into the reader.
    // For downloads into the reader directly extra runnouts won't matter.
    g_byte_array_append(telecode,runouts,16);

    skipping = FALSE;
    
    cp = utf8text;
    while(*cp)
    {
	if(*cp < 0x80)
	{
	    utf8char[0] = *cp;
	    utf8char[1] = 0x00;
	    //printf("UTF8=(0x%2X) ",*cp);
	}
	else
	{
	    //printf("UTF8=(0x%02X %02X) ",*cp,*(cp+1));
	    utf8char[0] = *cp++;
	    utf8char[1] = *cp;
	    utf8char[2] = 0x00;
	}

	if(!skipping)
	{
	    if(strcmp((gchar *) utf8char,"#") == 0)
	    {
		
		printf("skipping TRUE\n");
		skipping = TRUE;
		cp++;

		//Need to check for any corner cases in this code
		
		// Remove any spaces before the comment start by
		// reducing the byte array size
		while(telecode->data[telecode->len-1] == 0x1C)
		{
		    telecode->len -= 1;
		}

		// Check if last non-space before # is a linefeed
		// And set flag to avoid empty lines due to comments
		if(telecode->data[telecode->len-1] == 0x1E)
		{
		    ignoreLF = TRUE;
		}
		else
		{
		    ignoreLF = FALSE;
		}		
		continue;
	    }
	}
	else
	{
	    if(strcmp((gchar *) utf8char,"\n") == 0)
	    {
		printf("skipping FALSE\n");
		skipping = FALSE;
		if(ignoreLF)
		{
		    cp++;
		    continue;
		}
	    }
	    else
	    {
		cp++;
		continue;
	    }
	}


	
	
	for(n=0;n<64;n++)
	{
	    if( (convert2[n] != NULL) && (strcmp((gchar *) utf8char,convert2[n]) == 0))
	    {
		m = n & 0x1F;
		if(n > 58)
		{
		    // Shift independent
		    if(n == 62)
		    {
			g_byte_array_append(telecode,&shifts[2],1);
			printf("1D ");
			count++;
		    }
		    g_byte_array_append(telecode,&m,1);
		    printf("%02X ",m);
		    count++;
		}	
		else if((n > 31) && (n <=58))
		{
		    // Letter
		    if(!letters)
		    {
			g_byte_array_append(telecode,&shifts[4],1);
			printf("1F ");
			count++;
			letters = TRUE;
			figures = FALSE;
		    }
		    g_byte_array_append(telecode,&m,1);
		    printf("%02X ",n & 0x1F);
		    count++;
		}
		else if((n > 0) && (n < 32))
		{
		    // Figures
		    if(!figures)
		    {
			g_byte_array_append(telecode,&shifts[0],1);
			printf("1B ");
			count++;
			letters = FALSE;
			figures = TRUE;
		    }
		    g_byte_array_append(telecode,&m,1);
		    printf("%02X ",n & 0x1F);
		    count++;
		}
		break;
	    }
	}
	cp++;
    }

    g_byte_array_append(telecode,runouts,16);
    
    printf("\nConverted %d to %d (%d) characters\n",length,count,telecode->len);

    return telecode;
}




gboolean on_editorTextView_key_press_event(__attribute__((unused))GtkWidget *widget,
					   GdkEventKey *event)
{
    //int n;
    gboolean ignore;

    //printf("keyval=%x\n",event->keyval);

    ignore = notTelecode(event,FALSE);
    //telecode = 0xFF;

    // Convert a-z into A-Z
    if((event->keyval >= GDK_KEY_a) && (event->keyval <= GDK_KEY_z))
    {
	{	
	    GtkTextBuffer *buffer;
	    gchar upper;
	    upper = (gchar) ('A' + (event->keyval - GDK_KEY_a));
	    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
	    gtk_text_buffer_insert_at_cursor (buffer,&upper, 1);
                
	    return GDK_EVENT_STOP;    /* return TRUE - no further processing */
	}
    }
    return ignore;
}


    
#if 0    
    if(editorOnline)
    {
	if(ignore) return GDK_EVENT_STOP;


	if(ELLIOTTcode)
	{
	    if((event->keyval >= GDK_KEY_A) && (event->keyval <= GDK_KEY_Z))
	    {
		telecode = 1 + event->keyval - GDK_KEY_A ;
		telecode += 32;
	    }
	    else if((event->keyval >= GDK_KEY_a) && (event->keyval <= GDK_KEY_z))
	    {
		telecode = 1+ event->keyval - GDK_KEY_a;
		telecode += 32;


	    }
	    else
	    {
		for(n=0;n<32;n++)
		    if(keyToTelecode[n] == event->keyval)
		    {
			//if(n == 29) n = 30;   // CR -> LF
			if(n == 29) forceShift = TRUE;
			telecode = n;
			if(n >= 27) telecode += 64;
		    }
	    
	    }
	}

	if(MURRYcode)
	{
	    for(n=0;n<64;n++)
	    {
		if(keyToMurryCode[n] == event->keyval)
		{
		    if(n == 2) forceShift = TRUE;
		    telecode = n;
		    if((n == 4)||(n == 8)||(n == 2)) telecode += 64;
		}

	    }
	
	}
	if(forceShift && (telecode < 64))
	{
	    /*
	    gchar shift;
	    shift = 0x5B + ((telecode >> 3) & 0x4);
	    value[0] = 0x8A;
	    value[1] = shift;
	    printf("%s telecode = 0x%2X\n",__FUNCTION__,telecode & 0xFF);

	    g_io_channel_write_chars(E803_channel,value,2,&written,&error);
	    if(error != NULL) printf("ERROR !=NULL %s %d\n",__FUNCTION__,__LINE__);
	    g_io_channel_flush(E803_channel,NULL);
	    */
	    telecode += 0x80;
	    forceShift = FALSE;
	}
	    
	    
       	value[0] = '\x8A';
	value[1] = (gchar) (telecode & 0xFF);
	printf("%s telecode = 0x%2X\n",__FUNCTION__,telecode & 0xFF);

	g_io_channel_write_chars(E803_channel,value,2,&written,&error);
	if(error != NULL) printf("ERROR !=NULL %s %d\n",__FUNCTION__,__LINE__);
	g_io_channel_flush(E803_channel,NULL);


	return GDK_EVENT_STOP;
    }
    else

    {
	// Convert a-z into A-Z
	if((event->keyval >= GDK_KEY_a) && (event->keyval <= GDK_KEY_z))
	{
	    {	
                GtkTextBuffer *buffer;
                gchar upper;
		upper = (gchar) ('A' + (event->keyval - GDK_KEY_a));
                buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
		gtk_text_buffer_insert_at_cursor (buffer,&upper, 1);
                
                return GDK_EVENT_STOP;    /* return TRUE - no further processing */
	    }
	}
	return ignore;
    }
 
}
#endif

gboolean
on_editorNewButton_clicked(__attribute__((unused)) GtkButton *button,
			   __attribute__((unused))gpointer data)
{
    GtkTextIter start,end;

    gtk_text_buffer_get_start_iter (editorTextBuffer,&start);
    gtk_text_buffer_get_end_iter (editorTextBuffer,&end);

    gtk_text_buffer_delete (editorTextBuffer,&start,&end);
    
    gtk_widget_set_sensitive(editorSaveButton,FALSE);
    gtk_label_set_text(GTK_LABEL(editorFrameLabel),"Tape Editor");

    return  GDK_EVENT_PROPAGATE;
}


/*
  If a file with     ".utf8" extension is selected it is used 
  If a file without  ".utf8" extansion is selected and a ".utf8" version exists
     then ask user to choose which one to use
     otherwise load the telecode vversion 


 */


gboolean
on_editorOldButton_clicked(__attribute__((unused)) GtkButton *widget,
			   __attribute__((unused))gpointer data)
{
    //GtkDialog *dialog;
    gint res;
    gchar *text;
    const gchar *txt;
    gint ch;
    gsize length;
    gsize index;
    GError *error;
    gboolean letters,figures,useUTF8,returnFlag;
    //GtkTextIter end;
    GString *utf8Filename = NULL;
    char *filename;
    
    printf("%s called\n",__FUNCTION__);

    text = NULL;
    useUTF8 = FALSE;
    returnFlag = GDK_EVENT_PROPAGATE;
    
    res = gtk_dialog_run (GTK_DIALOG(loadFileChooserDialog));
    gtk_widget_hide(loadFileChooserDialog);
    
    if (res == GTK_RESPONSE_OK)
    {
	filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER (loadFileChooserDialog));

	// Check for ".utf8" extension, and  if present load the file directly into the text buffer

	const char *dot = strrchr(filename, '.');
	if((dot != NULL) && (strncmp(dot,".utf8",5) == 0))
	{
	    printf("This is a utf8 file\n");
	    useUTF8 = TRUE;
	}
	else
	{
	    // Add ".utf8" extension and check if it exists.

	    utf8Filename = g_string_new(filename);
	    g_string_append(utf8Filename,".utf8");

	    if(access(utf8Filename->str,R_OK) == 0)
	    {
		res = gtk_dialog_run(GTK_DIALOG(chooseFormatDialog));
		gtk_widget_hide(chooseFormatDialog);
		
		if(res == GTK_RESPONSE_NO)
		{ // Use UTF8 file
		    useUTF8 = TRUE;
		}
	    }
	}


	if(useUTF8)
	{
	    if(utf8Filename != NULL)
	    {
		g_file_get_contents (utf8Filename->str,&text,&length,&error);
	    }
	    else
	    {
		g_file_get_contents (filename,&text,&length,&error);
	    }

	    gtk_text_buffer_insert_at_cursor(editorTextBuffer,text,length);

	}
	else
	{
       	    error = NULL;
	    g_file_get_contents (filename,&text,&length,&error);

	    if(!isTelecodeTape(text,length))
	    {
		res = gtk_dialog_run (GTK_DIALOG (notTelecodeDialog));
		gtk_widget_hide(notTelecodeDialog);
		returnFlag = GDK_EVENT_STOP;
		goto cleanUp;
	    }

	    if(isBinaryTape(text,length))
	    {
		res = gtk_dialog_run(GTK_DIALOG(editBinaryDialog));
		gtk_widget_hide(editBinaryDialog);

		if(res == GTK_RESPONSE_CANCEL)
		{
		    returnFlag =  GDK_EVENT_STOP;
		    goto cleanUp;
		}
	    }

		
	
	    //gtk_text_buffer_get_end_iter (editorTextBuffer,&end);
	    letters = figures = FALSE;

	    for(index=0;index<length;index++)
	    {
		txt = NULL;
		ch = text[index] & 0x1F;
		if(ch >= 0x1B)
		{
		    switch(ch)
		    {
		    case 0x1B:
			figures = TRUE;
			letters = FALSE;
			txt = NULL;
			break;
		    case 0x1C:
			txt = " ";
			break;
		    case 0x1D:
			txt = "\n";
			break;
		    case 0x1E:
			break;
		    case 0x1F:
			figures = FALSE;
			letters = TRUE;
			break;
		    }
		}
		else
		{
		    if( letters || figures)
		    {
			if(letters) ch += 32;
			txt = convert2[ch];
		    }
		}
		//printf("txt=[%s]\n",txt);
		if(txt != NULL)
		{
		    gtk_text_buffer_insert_at_cursor(editorTextBuffer,txt,-1);
		}
	    }
	}
cleanUp:
	if(text) g_free(text);
	if(filename) g_free(filename);
	if(utf8Filename) g_string_free(utf8Filename,TRUE);
			 
    }
    return returnFlag ;
}

gboolean
on_editorDownloadButton_clicked(__attribute__((unused)) GtkButton *button,
				__attribute__((unused)) gpointer data)
{
    GByteArray *telecode;
    gchar value[3];
    GError *error = NULL;
    gsize written;
    
    if(editorDownloadBuffer != NULL)
	g_free(editorDownloadBuffer);
	

    telecode = convertToTelecode(FALSE);

    editorDownloadLength = telecode->len;
    editorDownloadBuffer = (gchar *) g_byte_array_free (telecode,FALSE);
    editorDownloaded = 0;

    downloadLengthp = &editorDownloadLength;
    downloadedp = &editorDownloaded;
    downloadBuffer = editorDownloadBuffer;
    

    value[0] = '\x80';
    value[1] = 0x00;
    value[2] = 0x00;
    
    g_io_channel_write_chars(E803_channel,value,3,&written,&error);
    g_io_channel_flush(E803_channel,NULL);

    // Save button states
    fileDownloadWasSensitive   = gtk_widget_get_sensitive(fileDownloadButton);
    editorDownloadWasSensitive = gtk_widget_get_sensitive(editorDownloadButton);
    
    gtk_widget_set_sensitive(fileDownloadButton,FALSE);
    gtk_widget_set_sensitive(editorDownloadButton,FALSE);
    return GDK_EVENT_PROPAGATE ;
}

gboolean
on_editorDownloadFromCursorButton_clicked(__attribute__((unused)) GtkButton *button,
				__attribute__((unused)) gpointer data)
{
    GByteArray *telecode;
    gchar value[3];
    GError *error = NULL;
    gsize written;
    
    if(editorDownloadBuffer != NULL)
	g_free(editorDownloadBuffer);
	

    telecode = convertToTelecode(TRUE);

    editorDownloadLength = telecode->len;
    editorDownloadBuffer = (gchar *) g_byte_array_free (telecode,FALSE);
    editorDownloaded = 0;

    downloadLengthp = &editorDownloadLength;
    downloadedp = &editorDownloaded;
    downloadBuffer = editorDownloadBuffer;
    

    value[0] = (gchar) 0x80;
    value[1] = 0x00;
    value[2] = 0x00;
    
    g_io_channel_write_chars(E803_channel,value,3,&written,&error);
    g_io_channel_flush(E803_channel,NULL);

    // Save button states
    fileDownloadWasSensitive   = gtk_widget_get_sensitive(fileDownloadButton);
    editorDownloadWasSensitive = gtk_widget_get_sensitive(editorDownloadButton);
    
    gtk_widget_set_sensitive(fileDownloadButton,FALSE);
    gtk_widget_set_sensitive(editorDownloadButton,FALSE);
    return GDK_EVENT_PROPAGATE ;
}

gboolean
on_editorSetFileButton_clicked(__attribute__((unused)) GtkButton *button,
			       __attribute__((unused))	gpointer data)
{
   
    gint res;
    
    printf("%s called\n",__FUNCTION__);

    
    
    res = gtk_dialog_run (GTK_DIALOG (saveFileChooserDialog));

    gtk_widget_hide(saveFileChooserDialog);
    
    if (res == GTK_RESPONSE_OK)
    {
	GString *title;

	if(editorFileName != NULL)
	    g_free (editorFileName);
	editorFileName = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (saveFileChooserDialog));
	
	title = g_string_new("Editing: ");
	g_string_append(title,editorFileName);
	gtk_label_set_text(GTK_LABEL(editorFrameLabel),title->str);
	g_string_free(title,TRUE);

	gtk_widget_set_sensitive(editorSaveButton,TRUE);

    }
    return GDK_EVENT_PROPAGATE ;
}


// Also save the utf8 text added so that comments can be preserved
gboolean
on_editorSaveButton_clicked(__attribute__((unused)) GtkButton *button,
			    __attribute__((unused))gpointer data)
{
    GByteArray *telecode;
    GError *error = NULL;
    GString *utf8FileName;
    GtkTextIter start,end;
    guchar *utf8text;
    gint length,slength;
    
    printf("%s called\n",__FUNCTION__);

    telecode = convertToTelecode(FALSE);

    g_file_set_contents (editorFileName,(gchar *) telecode->data,telecode->len,&error);

    g_byte_array_free(telecode,TRUE);

    utf8FileName = g_string_new(editorFileName);
    g_string_append(utf8FileName,".utf8");

    gtk_text_buffer_get_start_iter (editorTextBuffer,&start);
    gtk_text_buffer_get_end_iter (editorTextBuffer,&end);

    utf8text =  (guchar *) gtk_text_buffer_get_text (editorTextBuffer,&start,&end,FALSE);
    // GOTCHA !!!  This returns uft8 character count NOT the byte count !!!
    length = gtk_text_buffer_get_char_count (editorTextBuffer);
    slength = strlen((const char *)utf8text);
    printf("length=%d slength=%d\n",length,slength);

    
    g_file_set_contents (utf8FileName->str,(gchar *) utf8text,slength,&error);

    g_string_free(utf8FileName,TRUE);
    g_free(utf8text);
    
    return GDK_EVENT_PROPAGATE ;
}



/******************************* Communications ****************************/
#if 0
#define GLIB_SYSDEF_POLLIN =1
#define GLIB_SYSDEF_POLLOUT =4
#define GLIB_SYSDEF_POLLPRI =2
#define GLIB_SYSDEF_POLLHUP =16
#define GLIB_SYSDEF_POLLERR =8
#define GLIB_SYSDEF_POLLNVAL =32
typedef enum
{
  G_IO_STATUS_ERROR,
  G_IO_STATUS_NORMAL,
  G_IO_STATUS_EOF,
  G_IO_STATUS_AGAIN
} GIOStatus;
typedef enum /*< flags >*/
{
  G_IO_IN       GLIB_SYSDEF_POLLIN,
  G_IO_OUT      GLIB_SYSDEF_POLLOUT,
  G_IO_PRI      GLIB_SYSDEF_POLLPRI,
  G_IO_ERR      GLIB_SYSDEF_POLLERR,
  G_IO_HUP      GLIB_SYSDEF_POLLHUP,
  G_IO_NVAL     GLIB_SYSDEF_POLLNVAL
} GIOCondition;
#endif

#if 0
static void
printGIOCondition(GIOCondition condition)
{

    printf("condition = 0x%x ",condition);
    if(condition & G_IO_IN)   printf("G_IO_IN ");
    if(condition & G_IO_OUT)  printf("G_IO_OUT ");
    if(condition & G_IO_PRI)  printf("G_IO_PRI ");
    if(condition & G_IO_ERR)  printf("G_IO_ERR ");
    if(condition & G_IO_HUP)  printf("G_IO_HUP ");
    if(condition & G_IO_NVAL) printf("G_IO_NVAL ");
   
}

static void
printGIOStatus(GIOStatus status)
{
    printf("status = 0x%x ",status);
    switch(status)
    {
    case  G_IO_STATUS_ERROR:
	printf("G_IO_STATUS_ERROR ");
	break;
    case  G_IO_STATUS_NORMAL:
	printf("G_IO_STATUS_NORMAL ");
	break;
    case  G_IO_STATUS_EOF:
	printf("G_IO_STATUS_EOF ");
	break;
    case  G_IO_STATUS_AGAIN:
	printf("G_IO_STATUS_AGAIN ");
	break;
    default:
	printf("UNKNOWN ");
	break;
    }
}
#endif

static gboolean
readPTSHandler(guchar rdChar)
{
    gchar writeChar,*cp;
    gsize cnt,n;
    gchar *buffer;
    GtkTextMark *mark;
    GtkTextIter enditer;
    int finished;
    gsize written;
    GError *error = NULL;
    GtkTextView *textview;
    GtkTextBuffer *textbuffer;
    static gboolean pletters = FALSE;
    static gboolean pfigures = FALSE;
    static gboolean rletters = FALSE;
    static gboolean rfigures = FALSE;
    static gboolean *letters;
    static gboolean *figures;
    const gchar *txt;
    //static int nn = 0;
    guint reversed;
    guint readChar;

    readChar = rdChar;

    /*
    nn++;
    if(nn == 16)
    {
	nn = 0;
	printf("\n");
    }
    */
    //printf("(%02X)",readChar);
    txt = NULL;
    if(readChar < 0x80)
    {
	if(readChar < 0x20)
	{
	    // Teleprinter Output
	    readChar &= 0x1F;
	    if(printing == TRUE)
	    {
		textview = teleprinterTextView;
		textbuffer = teleprinterTextBuffer;
	    }
	    else
	    {
		textview = NULL;
		textbuffer = NULL;
	    }
	    letters = &pletters;
	    figures = &pfigures;

	    if(punching)
	    {
		
		g_byte_array_append(punchingBuffer,&rdChar,1);
		printf("[%02X]",readChar);
	    }
	}
	else
	{
	    // Reader Echo
	    readChar &= 0x1F;
	    textbuffer = readerTextBuffer;
	    textview = readerTextView;
	    letters = &rletters;
	    figures = &rfigures;
	}

	
	if(ELLIOTTcode)
	{
	    if(readChar >= 0x1B)
	    {
		switch(readChar)
		{
		case 0x1B:
		    *figures = TRUE;
		    *letters = FALSE;
		    txt = NULL;
		    break;
		case 0x1C:
		    txt = " ";
		    break;
		case 0x1D:
		    txt = NULL;
		    break;
		case 0x1E:
		    txt = "\n";
		    break;
		case 0x1F:
		    *figures = FALSE;
		    *letters = TRUE;
		    break;
		}
	    }
	    else
	    {
		if( *letters || *figures)
		{
		    if(*letters) readChar += 32;
		    txt = convert2[readChar];
		}
	    }
	}
	if(MURRYcode)
	{
	    reversed = ((readChar & 1) << 4) +((readChar & 2) << 2) + (readChar & 4) +
		((readChar & 8) >> 2) + ((readChar & 16) >> 4);

	    
	    switch(reversed)
	    {
	    case 0x02:
		txt = "\n";
		break;

	    case 0x08:
		txt = "<cr>";
		break;
	    case 0x1B:
		    *figures = TRUE;
		    *letters = FALSE;
		break;
	    case 0x1F:
		    *figures = FALSE;
		    *letters = TRUE;
		break;
	    default:
		if( *letters || *figures)
		{
		    if(*letters) reversed += 32;
		    txt = convert3[reversed];
		}
		break;
	    }
	}
	
	//printf("txt=[%s]\n",txt);
	if( (txt != NULL) && (textview != NULL)) 
	{
	    mark = gtk_text_buffer_get_mark (textbuffer, "end");
	    gtk_text_buffer_get_iter_at_mark (textbuffer,&enditer,mark);
	    
	    gtk_text_buffer_insert(textbuffer,&enditer,txt,-1);
	    
	    gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW(textview), mark);
	}
    }
    else
    {   // char >= 0x80
	switch(readChar)
	{
	case 0x81:
	    finished = FALSE;

	    cnt = *downloadLengthp - *downloadedp;
	    if( cnt > 256)
	    {
		cnt = 256;
	    }

	    buffer = &downloadBuffer[*downloadedp];
	    
	    if(cnt > 0)
	    {
		
		//printf("Sending a block\n");
		n = cnt;
		cp = buffer;
		while(n--) *cp++ &= 0x1F;
		
		writeChar = '\x81';
		g_io_channel_write_chars(E803_channel,&writeChar,1,&written,&error);
		writeChar = (gchar) (cnt & 0xFF);
		g_io_channel_write_chars(E803_channel,&writeChar,1,&written,&error);
		
		g_io_channel_write_chars(E803_channel,buffer,(gssize)cnt,&written,&error);
		g_io_channel_flush(E803_channel,NULL);
		*downloadedp += cnt;

		n = cnt;
		cp = buffer;
		while(n--) *cp++ |= '\x80';
	    }
	    else
	    {
		finished = 1;
	    }
	    //printf("readerDownloaded=%d readerDownloadLength=%d\n",
	    //   readerDownloaded,readerDownloadLength);

	    if(*downloadLengthp != 0)
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(downloadProgressBar),
					      (gdouble)*downloadedp/(gdouble)*downloadLengthp);
	    
	    if(finished)
	    {
		writeChar = '\x82';
		//write(serial_fd,&writeChar,1);
		g_io_channel_write_chars(E803_channel,&writeChar,1,&written,&error);
		g_io_channel_flush(E803_channel,NULL);
				
		//gtk_widget_set_sensitive(skipButton,TRUE);
		//gtk_widget_set_sensitive(rewindButton,TRUE);
		//gtk_widget_set_sensitive(echoCheckbutton,TRUE);


		//gtk_widget_set_sensitive(*downloadButtonp,TRUE);
		//g_free(readerDownloadBuffer);
		//readerDownloadBuffer = NULL;

		// Restore button states
		gtk_widget_set_sensitive(fileDownloadButton,fileDownloadWasSensitive);
		gtk_widget_set_sensitive(editorDownloadButton,editorDownloadWasSensitive);
		
		printf("Completed\n");
	    }

	    break;
	default:

	    break;
	}
    }


	

    return TRUE;
    
}



/* 
The socket used for a network connection to an emulator is set non-blocking 
(see SetSocketNonBlockingEnabled) before the call to connect().  This means that connect()
returns immidiatley with errno set to EINPROGRESS which perror prints as "Operation now in progress".

There are three "watches" set up on "E803_channel" (see on_networkConnectButton_clicked()).
E803_errorHandler       G_IO_ERR | G_IO_HUP     Error or Hung Up
E803_messageHandler     G_IO_IN                 Readable
E803_connectedHandler   G_IO_OUT                Writeable

E803_connectedHandler is only used once when the channel first becomes writeable after it is created.
If this is called it means that the network connection has succeeded.  It returns FALSE to remove the watch
otherwise it is repeatedly called.

E803_errorHandler is called if connect() eventually fails.  It is called with condition set to G_IO_ERR and G_IO_HUP.

E803_messageHandler is called when there is incomming data to read on the channel.  It is also called 
if the other end of the socket is closed.  In this case the call to g_io_channel_read_chars() returns
the staus "G_IO_STATUS_EOF". 


 */


static gboolean
E803_messageHandler(GIOChannel *source,
			     __attribute__((unused)) GIOCondition condition,
			     __attribute__((unused)) gpointer data)
{
    guchar message;
    gsize length;
    GError *error = NULL;
    GIOStatus status;
 
    status = g_io_channel_read_chars(source,(gchar *)&message,1,&length,&error);
  
    if(status != G_IO_STATUS_NORMAL)
    {
	printf("Rx=%d Tx=%d Er=%d\n",RxWatchId,TxWatchId,ErWatchId);
	// Remove the error source
	g_source_remove(ErWatchId);
	g_io_channel_shutdown(source,FALSE,NULL);
	g_io_channel_unref(E803_channel);
	E803_channel = NULL;
	// Returning FALSE will remove the recieve source

	//gtk_widget_show (connectionWindow);
	gtk_button_set_label(GTK_BUTTON(reconnectButton),"gtk-connect");
	reconnectButtonState = FALSE;
	gtk_window_set_title(GTK_WINDOW(mainWindow),"Disconnected");
	return FALSE;
    }
 
 
    readPTSHandler(message);
   
    return TRUE;
}




static gboolean
E803_connectedHandler( __attribute__((unused)) GIOChannel *source,
		       __attribute__((unused)) GIOCondition condition,
		       __attribute__((unused)) gpointer data)
{

    // When the connection is made, make it look like the user pressed the non-existent
    // "OK" button on the makingConnectionDialog.
    gtk_dialog_response(GTK_DIALOG(makingConnectionDialog),GTK_RESPONSE_OK);
   
    // Handler has done it's work so return FALSE to remove source.
    return FALSE;
}

static gboolean
E803_errorHandler( __attribute__((unused)) GIOChannel *source,
		       __attribute__((unused)) GIOCondition condition,
		       __attribute__((unused)) gpointer data)
{
    g_source_remove(RxWatchId);
    g_source_remove(TxWatchId);

    
    g_io_channel_shutdown(source,FALSE,NULL);
    g_io_channel_unref(E803_channel);
    E803_channel = NULL;
    // When the connection fails, make it look like the user pressed the non-existent
    // "REJECT" button on the makingConnectionDialog.
    gtk_dialog_response(GTK_DIALOG(makingConnectionDialog),GTK_RESPONSE_REJECT);
    
    // Handler has done it's work so return FALSE to remove source.
    return FALSE;
}





/* Returns true on success, or false if there was an error */
static gboolean
SetSocketNonBlockingEnabled(int fd, gboolean blocking)
{
   if (fd < 0) return FALSE;


   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) return FALSE;
   flags = blocking ? (flags | O_NONBLOCK) :  (flags & ~O_NONBLOCK) ;
   return (fcntl(fd, F_SETFL, flags) == 0) ? TRUE : FALSE;

}



gboolean
on_networkConnectButton_clicked(__attribute__((unused)) GtkButton *button,
				     __attribute__((unused)) gpointer data)
{
    GString *address,*title;
    int E803_socket,n,result;
    struct hostent *hp;
    struct sockaddr_in E803_server_name;
    gboolean localHost = FALSE;

    localHost = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(useLocalHost));
    
    address = g_string_new(NULL);

    if(localHost)
    {
	g_string_printf(address,"localhost");
    }
    else
    {
	g_string_printf(address,"%d.%d.%d.%d",
			(int)gtk_adjustment_get_value(ipAdjustments[0]),
			(int)gtk_adjustment_get_value(ipAdjustments[1]),
			(int)gtk_adjustment_get_value(ipAdjustments[2]),
			(int)gtk_adjustment_get_value(ipAdjustments[3]));
    }

    printf("Address=%s\n",address->str);
    /* Create socket on which to send and recieve. */
    E803_socket = socket(AF_INET, SOCK_STREAM, 0); 
    if (E803_socket < 0) { 
	perror("opening network socket to the E803");
	g_string_free(address,TRUE);
	return GDK_EVENT_PROPAGATE ; 
    } 

    SetSocketNonBlockingEnabled(E803_socket,TRUE);
    
    hp = gethostbyname(address->str); 
    if (hp == 0) { 
	fprintf(stderr, "%s: unknown host", address->str);
	g_string_free(address,TRUE);
	return GDK_EVENT_PROPAGATE ; 
    } 
    
    bcopy(hp->h_addr, &E803_server_name.sin_addr, (size_t) hp->h_length); 
    E803_server_name.sin_family = AF_INET; 
    E803_server_name.sin_port = htons(7000);  ; 
    
    n = connect(E803_socket, (struct sockaddr *)&E803_server_name,
	      sizeof (E803_server_name));
  
  
    if(n == -1)
    {
	perror("E803 connect returned:");
    }

    E803_channel = g_io_channel_unix_new(E803_socket);
    ErWatchId = g_io_add_watch(E803_channel,G_IO_ERR | G_IO_HUP ,E803_errorHandler,NULL);
    RxWatchId = g_io_add_watch(E803_channel,G_IO_IN ,E803_messageHandler,NULL);
    TxWatchId = g_io_add_watch(E803_channel,G_IO_OUT ,E803_connectedHandler,NULL);
    // Watches have incremented the ref count so OK to do this now.
    // This causes a secfault on Raspbian when disconnectin gfrom the emulator
    //g_io_channel_unref(E803_channel);

    g_io_channel_set_encoding(E803_channel,NULL,NULL);
    // 24/11/18
    g_io_channel_set_buffered(E803_channel,FALSE);

    
    
   


    result = gtk_dialog_run (GTK_DIALOG (makingConnectionDialog));
     printf("result = %d ",result);
    switch(result)
    {
    case GTK_RESPONSE_NONE:
	printf("GTK_RESPONSE_NONE");
	break;
	
    case GTK_RESPONSE_REJECT:
	// From error handler when connection fails.  Already tidied up.
	printf("GTK_RESPONSE_REJECT");
	break;
    case GTK_RESPONSE_ACCEPT:
	printf("GTK_RESPONSE_ACCEPT");
	break;
    case GTK_RESPONSE_DELETE_EVENT:
	printf("GTK_RESPONSE_DELETE_EVENT");
	break;
    case GTK_RESPONSE_OK:
	// From connected handler.
	gtk_widget_hide(connectionWindow);
	if(!gtk_widget_get_visible(mainWindow))
	{
	     gtk_widget_show(mainWindow);
	}
	title = g_string_new(NULL);
	g_string_printf(title,"Connected to %s",address->str);
	gtk_window_set_title(GTK_WINDOW(mainWindow),title->str);
	g_string_free(title,TRUE);
				 
	gtk_button_set_label(GTK_BUTTON(reconnectButton),"gtk-disconnect");
	reconnectButtonState = TRUE;

	
	printf("GTK_RESPONSE_OK");
	break;
    case GTK_RESPONSE_CANCEL:
	// User has aborted th econnection so need to tidy up here.
	printf("GTK_RESPONSE_CANCEL:");
	g_source_remove(RxWatchId);
	g_source_remove(TxWatchId);
	g_source_remove(ErWatchId);
	g_io_channel_shutdown(E803_channel,FALSE,NULL);
	g_io_channel_unref(E803_channel);
	E803_channel = NULL;
	break;
    case GTK_RESPONSE_CLOSE:
	printf("GTK_RESPONSE_CLOSE");
	break;
    case GTK_RESPONSE_YES:
	printf("GTK_RESPONSE_YES");
	break;
    case GTK_RESPONSE_NO:
	printf("GTK_RESPONSE_NO");
	break;
    case GTK_RESPONSE_APPLY:
	printf("GTK_RESPONSE_APPLY");
	break;
    case GTK_RESPONSE_HELP:
	printf("GTK_RESPONSE_HELP");
	break;
    default:
	printf("Unrecognised Responce");
	break;
    }
    printf("\n");

   
    g_string_free(address,TRUE);
    gtk_widget_hide(makingConnectionDialog);
    
    return GDK_EVENT_PROPAGATE ;
}


static
void populateSerialList(GtkBuilder *builder)
{
    int n;
    char *realname,*base;
    GString *fullpath;
    GtkListStore *store;
    GtkTreeIter iter;
    glob_t globbuf;

    store = GTK_LIST_STORE(gtk_builder_get_object_checked (builder, "serialDeviceListStore"));
	
    fullpath = g_string_new(NULL);

    // Find all the ttyUSB* and ttyS* devices in /dev
    globbuf.gl_offs = 0;
    glob("/dev/ttyUSB*", GLOB_DOOFFS, NULL, &globbuf);
    glob("/dev/ttyS*", GLOB_DOOFFS | GLOB_APPEND, NULL, &globbuf);

    n = 0;
    while(globbuf.gl_pathv[n] != NULL)
    {
	// Workout if there is real hardware for each device...

	g_string_printf(fullpath,"/sys/class/tty/%s/device/subsystem",&globbuf.gl_pathv[n][5]);
	realname  = realpath(fullpath->str,NULL);
	base = g_path_get_basename(realname);

	// If value is "platform" it is NOT a real interface.
	if(strcmp(base,"platform") != 0)
	{
	    gtk_list_store_append (store, &iter);
	    gtk_list_store_set (store, &iter,
				0,globbuf.gl_pathv[n] ,
				-1);
	}
	g_free(base);
	free(realname);
	n += 1;
    }

    globfree(&globbuf);
    
    gtk_combo_box_set_active(connectionCombobox,0);
}
    

// Helper 
GObject *gtk_builder_get_object_checked(GtkBuilder *builder,const gchar *name)
{
    GObject *gotten;

    gotten = gtk_builder_get_object (builder,name);
    if(gotten == NULL)
    {
	printf("FAILED TO GET (%s)\n",name);
    }
    return gotten;
}
    
// CSS to override stupid wildcard defaults set on Raspberry Pi Desktop.
const gchar *css =
"\
textview {\
    font-family: monospace;\
}\
";

int main(int argc,char **argv)
{
    struct passwd *pw;
    uid_t uid;
    gboolean createConfigDirectory = FALSE;
    int  a1,a2,a3,a4;
    gboolean addressOK = FALSE;
    
    GtkBuilder *builder;
    GString *adjustmentName;
    int n;
    GtkTextIter iter;
    GdkDisplay *display;
    GdkScreen *screen;
    GtkCssProvider *provider;
    GError *error;

    //GList *recent;
    
    gtk_init (&argc, &argv);

    // Install simple logging to stdout.
    LoggingInit();
    
    /* Set global path to user's configuration and machine state files */
    uid = getuid();
    pw = getpwuid(uid);

    configPath = g_string_new(pw->pw_dir);
    configPath = g_string_append(configPath,"/.PLTS/");

        // Now Check it exists.   If it is missing it is not an
    // error as it may be the first time this user has run the emulator.
    {
	GFile *gf = NULL;
	GFileType gft;
	GFileInfo *gfi = NULL;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path(configPath->str);
	gfi = g_file_query_info (gf,
				 "standard::*",
				 G_FILE_QUERY_INFO_NONE,
				 NULL,
				 &error2);

	if(error2 != NULL)
	{
	    g_warning("Could not read user configuration directory: %s\n", error2->message);
	    createConfigDirectory = TRUE;
	}
	else
	{
	
	    gft = g_file_info_get_file_type(gfi);

	    if(gft != G_FILE_TYPE_DIRECTORY)
	    {
		g_warning("User's configuration directory (%s) is missing.\n",configPath->str);
		createConfigDirectory = TRUE;
	    }
	}
	if(gfi) g_object_unref(gfi);
	if(gf) g_object_unref(gf);

    }

    if(createConfigDirectory == TRUE)
    {
	GFile *gf;
	GError *error2 = NULL;
	
	gf = g_file_new_for_path(configPath->str);
	
	g_file_make_directory (gf,
                       NULL,
                       &error2);

	
	if(error2 != NULL)
	{
	    g_error("Could not create  directory:%s\n", error2->message);
	}

	g_object_unref(gf);

    }
    else
    {
	GString *configFileName;
	GIOChannel *file;

	GIOStatus status;
	gchar *message;
	gsize length,term;

	

	configFileName = g_string_new(configPath->str);
	g_string_append(configFileName,"DefaultIP");

	if((file = g_io_channel_new_file(configFileName->str,"r",&error)) == NULL)
	{
	    g_warning("failed to open file %s due to %s\n",configFileName->str,error->message);
	    
	}
	else
	{
	    while((status = g_io_channel_read_line(file,&message,&length,&term,&error)) 
		  == G_IO_STATUS_NORMAL)
	    {
		if(message != NULL)
		{
		    g_info("read %s from congif file\n",message);

		    if(sscanf(message,"%d.%d.%d.%d\n",&a1,&a2,&a3,&a4) == 4)
		    {
			g_info("Parsed as %d %d %d %d\n",a1,a2,a3,a4);
			addressOK = TRUE;
		    }
		    else
		    {
			g_info("Failed to parse default address from %s\n",message);
		    }
	
		    g_free(message);
		}
	    }
	    g_io_channel_shutdown(file,FALSE,NULL);
	    g_io_channel_unref(file);
	}
    }



    


   
    

    recentManager = gtk_recent_manager_get_default ();
/*
    recent = gtk_recent_manager_get_items(recentManager);
    while(recent != NULL)
    {
	info = (GtkRecentInfo *) recent->data;
	printf("recent=(%s) (%s) (%s)\n",
	       gtk_recent_info_get_uri_display(info),
	       gtk_recent_info_get_description(info),
	       gtk_recent_info_last_application(info));
	recent = g_list_next(recent);
    }
*/

    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "PLTS3.glade", NULL);
    
    // Set globals for widgets.
    GETWIDGET(connectionCombobox,GTK_COMBO_BOX);
    GETWIDGET(connectionWindow,GTK_WIDGET);
    GETWIDGET(mainWindow,GTK_WIDGET); 

    adjustmentName =  g_string_new(NULL);
    for(n=0;n<4;n++)
    {
	g_string_printf(adjustmentName,"adjustment%d",n+1);
	
	ipAdjustments[n] = GTK_ADJUSTMENT(gtk_builder_get_object_checked (builder, adjustmentName->str));
    }
    g_string_free(adjustmentName,TRUE);

    // Set address widgets to the default address.
    if(addressOK)
    {
	gtk_adjustment_set_value(ipAdjustments[0],(gdouble)a1);
	gtk_adjustment_set_value(ipAdjustments[1],(gdouble)a2);
	gtk_adjustment_set_value(ipAdjustments[2],(gdouble)a3);
	gtk_adjustment_set_value(ipAdjustments[3],(gdouble)a4);
    }

    GETWIDGET(makingConnectionDialog,GTK_WIDGET);
    GETWIDGET(reconnectButton,GTK_WIDGET);

    GETWIDGET(openRecentFileChooserDialog,GTK_WIDGET);
    GETWIDGET(fileDownloadButton,GTK_WIDGET);
    GETWIDGET(fileDownloadFrameLabel,GTK_WIDGET);
    GETWIDGET(fileTooBigDialog ,GTK_WIDGET);
    GETWIDGET(notTelecodeDialog,GTK_WIDGET);
    GETWIDGET(chooseFormatDialog,GTK_WIDGET);
    GETWIDGET(tapeImageDrawingArea,GTK_WIDGET);
    GETWIDGET(readerEchoButton,GTK_WIDGET);
    GETWIDGET(editorDownloadButton,GTK_WIDGET);
    GETWIDGET(loadFileChooserDialog,GTK_WIDGET);
    GETWIDGET(teleprinterTextView,GTK_TEXT_VIEW);
    GETWIDGET(teleprinterTextBuffer,GTK_TEXT_BUFFER);
    GETWIDGET(readerTextView,GTK_TEXT_VIEW);
    GETWIDGET(readerTextBuffer,GTK_TEXT_BUFFER);

    gtk_text_buffer_get_end_iter(teleprinterTextBuffer, &iter);
    gtk_text_buffer_create_mark(teleprinterTextBuffer, "end", &iter, FALSE);

    gtk_text_buffer_get_end_iter(readerTextBuffer, &iter);
    gtk_text_buffer_create_mark(readerTextBuffer, "end", &iter, FALSE);


    GETWIDGET(downloadProgressBar,GTK_WIDGET);

    GETWIDGET(windUpFromStartButton,GTK_WIDGET);
    GETWIDGET(windUpFromEndButton,GTK_WIDGET);
    GETWIDGET(discardTapeButton,GTK_WIDGET);
    GETWIDGET(saveFileChooserDialog,GTK_WIDGET);
    GETWIDGET(punchingToTapeButton,GTK_WIDGET);

    GETWIDGET(editorTextBuffer,GTK_TEXT_BUFFER);
    GETWIDGET(editorSaveButton,GTK_WIDGET);
    GETWIDGET(editorFrameLabel,GTK_WIDGET);
    GETWIDGET(editBinaryDialog,GTK_WIDGET);
    GETWIDGET(useLocalHost,GTK_WIDGET);
	       
    // Override textview style set by Raspberr Pi Desktop theme 
    display = gdk_display_get_default();
    screen = gdk_display_get_default_screen(display);
    provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    error = NULL;
    gtk_css_provider_load_from_data(provider, css,-1, &error);
    
    /*
    GETWIDGET(,GTK_WIDGET);
    */
    populateSerialList(builder);
 
    gtk_builder_connect_signals (builder, NULL);

    gtk_widget_show (connectionWindow);

    gtk_main ();
}
