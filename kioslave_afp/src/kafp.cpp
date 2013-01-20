
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <kmainwindow.h>
#include <klocale.h>

#include <qtimer.h>
#include <qcstring.h>
#include <qsocket.h>
#include <qdatetime.h>
#include <qbitarray.h>

#include <kapplication.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kinstance.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <klocale.h>
#include <kurl.h>
#include <kde_file.h>

#include <qlabel.h>
#include <qlineedit.h>
#include <qcombobox.h>
#include <qbuttongroup.h>
#include <qlistbox.h>
#include <qdir.h>

#include <ksock.h>
#include <libafpclient.h>
#include <afp.h>
#include <afpsl.h>
#include "afpsl.h"
#include "afpfsd.h"

#include "afploginwidget.h"
#include "kafp.h"
#include "map_def.h"

#include <kdialog.h>
#include <klocale.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

#include <kdebug.h>
#include <kio/job.h>
#include <kmimetype.h>
#include <kprotocolinfo.h>

#include <qapplication.h>
#include <qeventloop.h>

// The length of log messages on connect
#define CONNECT_LOG_LEN 1024

using namespace KIO;

afpURL::afpURL(const KURL &kurl)
{
    p_volumename = kurl.path().section( '/', 1, 1 );  
    p_shortpath = kurl.path().section( '/', 2, 99,2); 
}

QString afpURL::afp_path(void)
{
   return p_shortpath;
}

QString afpURL::volumename(void)
{
   return p_volumename;
}


kio_afpProtocol::kio_afpProtocol( 
	const QCString &protocol,
	const QCString &poolSocket,
	const QCString &appSocket) :
    QObject(), 
	SlaveBase(protocol,poolSocket,appSocket)
{
    m_bAttached=false;

#define VOLUME_DIR_NAME ".afpvolumes"


    infoMessage(i18n("Connected to afpfsd"));
    
    m_bLoggedOn = false;
    m_bAttached = false;
    m_mountpointPrepared=false;
    mountpoint=QDir::root();
}

kio_afpProtocol::~kio_afpProtocol()
{
    kdDebug() << "\n\nkio_afpProtocol::~kio_afpProtocol()" << endl;
}

bool kio_afpProtocol::prepare_mountpoint(const char * t)
{
    QString tvolumename(t);

kdDebug(7101) << "starting with " << mountpoint.path() << endl;
kdDebug(7101) << "for volume " << tvolumename << endl;

    if (m_mountpointPrepared) return true;

    mountpoint = QDir::home();

    if (mountpoint.cd(VOLUME_DIR_NAME,false)==false) {
        mountpoint.mkdir(VOLUME_DIR_NAME,false);
        if (mountpoint.cd(VOLUME_DIR_NAME,false)==false) {
		return false;
            mountpoint=QDir::root();
        }
    }
    // See if the straight volumename exists and works
    if ((mountpoint.cd(tvolumename,false)==true) && 
         (mountpoint.isReadable()==true)) {
        goto done;

    }

    // Check to see if there's another volume mounted here already

    // Try creating the straight volumename
    if ((mountpoint.mkdir(tvolumename,false)==true) && 
         (mountpoint.cd(tvolumename,false)==true) &&
         (mountpoint.isReadable()==true)) {
        goto done;

    }

    // Try volumename-i, and count up i
    for (int i=0;i<65535;i++) {
        QString tmpvolumename = tvolumename;
        tmpvolumename.append("-");
	tmpvolumename.append(i);
        if ((!mountpoint.exists(tmpvolumename,false)) &&
           (mountpoint.mkdir(tmpvolumename,false)==true)) {
               if (mountpoint.cd(tmpvolumename,false)==false) goto error;
               if (mountpoint.isReadable()==false) goto error;
               goto done;
        }

    }

error:
printf("error occured preparing mountpoint\n");
    return false;

done:
kdDebug(7101) << "mountpoint " << mountpoint.path() << endl;
    m_mountpointPrepared=true;
printf("created mountpoint\n");
    return true;
}


void kio_afpProtocol::update_logmessage(QString new_message)
{
	status_line_text.append(new_message);
	dlg->status_line->setText(status_line_text);
}

void kio_afpProtocol::clear_logmessage(void)
{
	status_line_text=""; 
	dlg->status_line->setText("");
}

QString kio_afpProtocol::translated_path(const QString gotpath)
{
kdDebug(7101) << "Starting with " << gotpath << endl;
	QString tmppath = mountpoint.path();
kdDebug(7101) << "now " << tmppath << endl;
	tmppath.append("/");
kdDebug(7101) << "now " << tmppath << endl;
	tmppath.append(gotpath);
kdDebug(7101) << "now " << tmppath << endl;
	return tmppath;

}


void kio_afpProtocol::do_attach(void)
{
	int ret;
	volumeid_t tmp_volumeid;


kdDebug(7101) << "cur: " << dlg->volume_list->currentText() << endl;

	snprintf(m_url.volumename, AFP_VOLUME_NAME_LEN,"%s",
		(const char *) dlg->volume_list->currentText());

printf("volumename: %s\n",m_url.volumename);
        if (prepare_mountpoint(m_url.volumename)==false) {
printf("Could not mount because mountpoint is broken\n");
		return;
	}

	ret = afp_sl_mount( &m_url,mountpoint.path(),
		AFP_MAPPING_UNKNOWN,DEFAULT_MOUNT_FLAGS);

	if ((ret==AFP_SERVER_RESULT_OKAY) ||
	(ret==AFP_SERVER_RESULT_ALREADY_ATTACHED)) {
		dlg->attach->setEnabled(false);
		dlg->detach->setEnabled(true);
		memcpy(&volumeid,&tmp_volumeid,sizeof(volumeid_t));
		m_bAttached=true;
	}

printf("ret from mount: %d\n",ret);

}

bool kio_afpProtocol::handle_connect_errors(int ret)
{
	QString error_message;
	bool fatalerror = false;
	switch (ret) {
	case 0: 
		error_message=i18n("Connection succeeded.");
		break;
	case -EACCES: 
		error_message=i18n("Login incorrect");
		break;
	case -ENONET: 
		error_message=i18n("Could not get address of server");
printf("ENONET\n");
		fatalerror=true;
		break;
	case -ETIMEDOUT:
		error_message=i18n("Timeout connecting to server");
		fatalerror=true;
		break;
	case -EHOSTUNREACH:
		error_message=i18n("No route to host");
		fatalerror=true;
		break;
	case -ECONNREFUSED:
		error_message=i18n("Connection refused");
		fatalerror=true;
		break;
	case -ENETUNREACH:
		error_message=i18n("Server unreachable");
		fatalerror=true;
		break;
	default:
		/* Internal error */
		error_message=i18n("Internal error");
		fatalerror=true;
	}

	if (fatalerror) {
printf("fatal error\n");
		QMessageBox::critical( 0, "AFP kioslave",
			QString(i18n("Could not connect to server.\n")+
				error_message));
	}
	return fatalerror;
}

void kio_afpProtocol::volume_list_attach(QListBoxItem *) 
{
	printf("volume attached\n");

}

void kio_afpProtocol::do_connect(void)
{
	int ret;
	unsigned int uam_mask=default_uams_mask();
	int err;
	unsigned int textlen = CONNECT_LOG_LEN;
	char text[CONNECT_LOG_LEN];

printf("**** do connect\n");

	snprintf(m_url.username,AFP_MAX_USERNAME_LEN,"%s", 
		(const char * ) dlg->username->text());
	snprintf(m_url.password,AFP_MAX_PASSWORD_LEN,"%s", 
		(const char * ) dlg->password->text());

	clear_logmessage();
	QString logmessage;

	logmessage = i18n("Logging in as user %1...");
	logmessage.arg( m_url.username);
	update_logmessage(logmessage);

	memset(text,0,textlen);
	ret = afp_sl_connect(&m_url,uam_mask,NULL,
		text,&err);
	if (handle_connect_errors(ret)) {
printf("fatal connect\n");
		return;
	}

	if (strlen(text)>0) {
		dlg->login_message->setEnabled(true);
		dlg->login_message->setText(text);
	}

	QString error_message;
	infoMessage(text);

	snprintf(m_servername,AFP_SERVER_NAME_LEN,"%s",m_url.servername);

	clear_logmessage();
	logmessage.sprintf("Logged in as user %s on server %s using UAM %s.",
		m_url.username,m_servername,"unknown");
	update_logmessage(logmessage);

	m_bLoggedOn=true;
kdDebug(7101) << "Now connected on " << m_servername << endl;
	showVolumes(&m_url);

	return;
}

void kio_afpProtocol::showVolumes(struct afp_url * afp_url)
{

	// Gray out login details
	dlg->attach->setEnabled(false);
	dlg->authentication->setEnabled(false);
	dlg->username->setEnabled(false);
	dlg->password->setEnabled(false);
	dlg->connect_button->setEnabled(false);
	dlg->disconnect->setEnabled(true);



	dlg->volume_list->setFocus();

	unsigned int num;

	struct afp_volume_summary volumes_on_server[KIO_NUM_VOLS];
	int ret;

	ret = afp_sl_getvols(afp_url,0,KIO_NUM_VOLS,
		&num,volumes_on_server);

	struct afp_volume_summary * v = volumes_on_server;
	for (unsigned int j=0;j<num;j++) {
		v=&volumes_on_server[j];
		char * name = v->volume_name_printable;
		printf("vol: %s, %c\n",name,v->flags);
		dlg->volume_list->insertItem(i18n(name));
	}
printf("enabling vols\n");
	dlg->volume_list->setEnabled(true);
printf("done\n");


}

bool kio_afpProtocol::logged_into_server(struct afp_url * afp_url)
{
	// We should really be looking in a list 
	//
	if (strncmp(m_servername, afp_url->servername,AFP_SERVER_NAME_LEN)==0)
		return true;

	return false;
}

bool kio_afpProtocol::attached_to_volume(struct afp_url * afp_url)
{
	if (afp_url->volumename==m_volumename) return true;

	else return false;
}

int kio_afpProtocol::afpLogin(struct afp_url * afp_url)
{
	int ret;
	unsigned int uam_mask=default_uams_mask();
	int err;
	QString errorMsg;
	char loginmesg[AFP_LOGINMESG_LEN];

kdDebug(7101) << "afpLogin" << endl;

	if (m_bLoggedOn)
		return 0;
printf("af1\n");

	if (logged_into_server(afp_url)) {
		m_bLoggedOn = true;
		infoMessage(i18n("Already logged on"));

		if (attached_to_volume(afp_url)) {
			m_bAttached = true;
			infoMessage(i18n("Already attached to volume"));
			return 0;
		}

	} 

	// Try to connect 
	infoMessage(i18n("Logging in..."));

	ret = afp_sl_connect(afp_url,uam_mask,NULL,
		loginmesg,&err);

	if (ret==AFP_SERVER_RESULT_AFPFSD_ERROR) {
		infoMessage(i18n("Could not continue, could not connect to afpfsd."));
		return -1;

	}

	if (handle_connect_errors(ret)) {
		m_bLoggedOn=false;
		printf("fatal error\n");
		return -1;
	}
printf("af9\n");

	if (ret == 0) m_bLoggedOn=true;

	infoMessage(i18n("Logged in properly."));

	// We're logged in, let's see if we've attached to a volume
printf("af10: %s\n",afp_url->volumename);

	if ((m_bLoggedOn) && (attachvolume(afp_url)==true)) {
		m_bAttached=true;
#if 0
	if ((strlen(afp_url->volumename)>0))
		memcpy(m_volumename,afp_url->volumename,
			AFP_VOLUME_NAME_LEN);
#endif
		return 0;
	}
printf("af18\n");

	/* Either we're not logged in or not attached, so show dialog */

	dlg = new afploginWidget();

	clear_logmessage();
	infoMessage(i18n("Opening dialog to mount volume"));
	update_logmessage(i18n("Getting server info..."));

	struct afp_server_basic server_basic;
	ret=afp_sl_serverinfo(afp_url,&server_basic);
	switch(ret) {
	case AFP_SERVER_RESULT_AFPFSD_ERROR: 
		infoMessage(i18n("Could not continue, could not connect to afpfsd."));
		return -1;
	case 0:
		break;
	default:
		printf("something wrong in serverinfo, %d\n",ret);
		return false;
	}

	
printf("af24\n");
	update_logmessage(i18n("Connected to server for status"));

	QPixmap q;
printf("comparing %d with %d, %d, %d\n",
	server_basic.server_type, AFPFS_SERVER_TYPE_NETATALK, 
AFPFS_SERVER_TYPE_AIRPORT, AFPFS_SERVER_TYPE_MACINTOSH);
printf("name: %s\n",server_basic.server_name_printable);

	switch (server_basic.server_type) {
	case AFPFS_SERVER_TYPE_NETATALK:
		q.load("/usr/share/icons/hicolor/128x128/afp/netatalk.png");
		break;
	case AFPFS_SERVER_TYPE_AIRPORT:
		q.load("/usr/share/icons/hicolor/128x128/afp/airport-extreme.png");
		break;
	case AFPFS_SERVER_TYPE_MACINTOSH:
		q.load("/usr/share/icons/hicolor/128x128/afp/macos-generic.png");
		break;
	default:
		break;
	}




	dlg->server_icon->setPixmap(q);


	dlg->username->setText(afp_url->username);

	dlg->connect_button->setAutoDefault(true);
	
	memcpy(&m_url,afp_url,sizeof(struct afp_url));

	connect(dlg->password,SIGNAL(returnPressed()), 
	this, SLOT(do_connect()));
	connect(dlg->connect_button,SIGNAL(clicked()), 
	this, SLOT(do_connect()));
	connect(dlg->attach,SIGNAL(clicked()), 
	this, SLOT(do_attach()));
	connect(dlg->volume_list,SIGNAL(returnPressed(QListBoxItem *)), 
	this, SLOT(volume_list_attach(QListBoxItem*)));
	connect(dlg->volume_list,SIGNAL(doubleClicked(QListBoxItem *)), 
	this, SLOT(volume_list_attach(QListBoxItem*)));

	for (int j=1;j<0x100;j<<=1)
		if (j & server_basic.supported_uams)
			dlg->uam->insertItem(uam_bitmap_to_string(j),-1);
printf("af30\n");

	if (m_bLoggedOn) {
		showVolumes(afp_url);
	}

	dlg->server_description->
		setText(i18n(server_basic.server_name_printable));
	QFont sansFont( "Helvetica [Cronyx]", 20);
	dlg->server_description->setFont(sansFont);
	dlg->exec();

	kdDebug(7101) << "username: " << dlg->username->text() << endl;

	clear_logmessage();
	update_logmessage(i18n("Ready for login"));

	errorMsg = i18n("Authentication failed or needed");
	delete(dlg);
printf("af35\n");

	return 0;
}


bool kio_afpProtocol::attachvolume( struct afp_url * afp_url) 
{

  int ret;
  volumeid_t tmp_volumeid;

kdDebug(7101) << "Attaching to volume" <<endl;

  if (afpLogin(afp_url))
     return false;

  if (m_bAttached)
     return true;

  if (prepare_mountpoint(afp_url->volumename)==false) {
printf("Could not mount because mountpoint is broken 2\n");
     return false;
  }

   ret = afp_sl_mount(&m_url,mountpoint.path(),
		AFP_MAPPING_UNKNOWN,DEFAULT_MOUNT_FLAGS);

  if (ret==AFP_SERVER_RESULT_AFPFSD_ERROR) {
	return false;
  }

  if ((ret==AFP_SERVER_RESULT_OKAY) ||
       (ret==AFP_SERVER_RESULT_ALREADY_ATTACHED)) {
        memcpy(&volumeid,&tmp_volumeid,sizeof(volumeid_t));
        return true;
  }

  kdDebug(7101) << "Problem attaching" << endl;

  return false;
}

void kio_afpProtocol::slave_status(void) {
kdDebug(7101) << "*** Getting slave status" << endl;
    slaveStatus(m_servername,1);
}


void kio_afpProtocol::get(const KURL& url )
{
    kdDebug(7101) << "\n\nget " << url.prettyURL() << endl;
    struct afp_url afp_url;
    int ret;
    unsigned int fileid;
    unsigned long long total=0;
    QString resumeOffset = metaData("resume");
    // bool ok;
    // struct stat stat;
    // KIO::fileoffset_t offset = resumeOffset.toLongLong(&ok);

    QString kafpurl = url.url(0,0);
    afp_default_url(&afp_url);
    afp_parse_url(&afp_url,kafpurl,0);

    if (attachvolume(&afp_url)==false) {
    kdDebug(7101) << "could not attach for get of " << url.prettyURL() << endl;
       finished();
       return;
    }

    ret=afp_sl_open(NULL,NULL,&afp_url, &fileid,0);


#if 0

    char buffer[GET_DATA_SIZE];
    QByteArray array;
    

    while (eof==0) {
       ret = afp_sl_read(&connection,&volid,fileid,0,
               offset, GET_DATA_SIZE,&received,&eof,buffer);

       array.setRawData(buffer,received);
       data(array);
       array.resetRawData(buffer,received);

       total+=received;
       offset=total;
    }


    infoMessage( i18n( "Retrieving %1 from %2...") 
      .arg(KIO::convertSize(retreived)) .arg(afp_url.servername));


#endif

    totalSize(total);

    ret=afp_sl_close(&volumeid,fileid);

}

void kio_afpProtocol::put(const KURL & // url
	, int // permissions
	, bool // overwrite
	, bool // resume
	)
{


}

bool kio_afpProtocol::listVolumes(const KURL & url)
{
  UDSEntry entry;
  struct afp_volume_summary vols[KIO_NUM_VOLS];
  int ret;
  struct afp_url afp_url;
  unsigned int num;
  QString kafpurl = url.url(0,0);
  afp_default_url(&afp_url);
  afp_parse_url(&afp_url,kafpurl,0);

    kdDebug(7101) << "*** list volumes " << endl ;
printf("lv1\n");

	if (afpLogin(&afp_url)) {
		kdDebug(7101) << "*** Cannot login, so can't list volumes" << endl ;
		return false;
	}

	ret = afp_sl_getvols(&afp_url,0,10,&num,vols);

	switch(ret) {
	case AFP_SERVER_RESULT_AFPFSD_ERROR:
		infoMessage(i18n("Could not continue, could not connect to afpfsd."));
		return false;
	case 0:
		break;
	default:
	     kdDebug(7101) << "problem with getvols, " << ret << endl;
	     return false;
	};

printf("got %d vols\n",num);

  struct afp_volume_summary *v;
  for (unsigned int i=0;i<num;i++) {
  //  char * name = data[i * AFP_VOLUME_NAME_LEN];
    v=&vols[i];
    char * name = v->volume_name_printable;
printf("volume: %s\n",name);

    entry.clear();
    UDSAtom atom;
    atom.m_uds = UDS_NAME;
    atom.m_str = name;
    entry.append( atom );

    atom.m_uds = KIO::UDS_FILE_TYPE;
    atom.m_long = S_IFDIR;
    entry.append( atom );

    atom.m_uds = KIO::UDS_ACCESS;
    atom.m_long = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    entry.append( atom );

    atom.m_uds = KIO::UDS_USER;
    atom.m_str = "root";
    entry.append( atom );
    atom.m_uds = KIO::UDS_GROUP;
    entry.append( atom );

    atom.m_uds = UDS_USER;
    atom.m_str = "root";
    entry.append( atom );

    atom.m_uds = UDS_GROUP;
    atom.m_str = "root";
    entry.append( atom );

    listEntry(entry,false);
  }
  listEntry(entry,true );
  statEntry( entry );

  return true;
}


int kio_afpProtocol::listRealDir( const KURL & url ) 
{
  afpURL afpurl(url);
  int ret;
  unsigned int numfiles, total=0;
  int eod;
  struct afp_file_info_basic * fpb;
  struct afp_url afp_url;

  kdDebug(7101) << "*** list realdir" << endl ;

  QString kafpurl = url.url(0,0);
  afp_default_url(&afp_url);
  afp_parse_url(&afp_url,kafpurl,0);

  if (attachvolume(&afp_url)==false) return -1;

  QString localpath(afp_url.path);
  QString newpath = translated_path(localpath);

kdDebug(7101) << "Translated path is " << newpath << endl;

  DIR *dp = NULL;
  KDE_struct_dirent *ep;

  dp = opendir(newpath.data());

  if (dp==0) {
printf("error opening dir %s\n",newpath.data());
    return -1;
  }

  UDSEntry entry;
  KDE_struct_stat s;

  char curdir[PATH_MAX];

  getcwd(curdir,PATH_MAX-1);

  chdir(newpath.data());

  while ((ep=KDE_readdir(dp))!=0) {
printf("got file %s\n",ep->d_name);
    entry.clear();

    QString localpath(ep->d_name);
    ret = KDE_lstat(localpath,&s);
    entry=statToEntry(&s);

    UDSAtom atom;

    atom.m_uds = UDS_NAME;
    atom.m_str = ep->d_name;
    entry.append( atom );

    listEntry(entry,false);

  }
    listEntry(entry,true);
printf("done with readdir\n");
  closedir(dp);

  chdir(curdir);

#if 0
  while(1) {
  kdDebug(7101) << "*** about to readdir " << endl ;
    ret = afp_sl_readdir(&connection,NULL,NULL,&afp_url,total,10,&numfiles,
		&fpb,&eod);
    total+=numfiles;
    if (ret!=AFP_SERVER_RESULT_OKAY) break;

    for (unsigned int i=0;i<numfiles;i++) {
      entry=FPToEntry(fpb);

      listEntry (entry, false);
      fpb++;
    }

    if (eod) break;
  }
  listEntry (entry, true);
#endif

  finished();

  return 0;
}

void kio_afpProtocol::listDir( const KURL & url )
{
    kdDebug(7101) << "\n\nlistdir " << url.prettyURL() << endl;

    if (url.path().isEmpty()) {
       listVolumes(url);
    } else {
       listRealDir(url);
    }
    finished();
    return;
}

void kio_afpProtocol::mkdir( const KURL &  url
	, int // permissions 
)
{
kdDebug(7101) << "\n\nmkdir " << url.prettyURL() << endl;
  finished();
}
void kio_afpProtocol::rename( const KURL &  src
	, const KURL & // dest
	, bool // overwrite 
	)
{
kdDebug(7101) << "\n\nrename " << src.prettyURL() << endl;
  finished();

}

void kio_afpProtocol::symlink(const QString & // target
	, const KURL & // dest
	, bool // overwrite
)
{

}

void kio_afpProtocol::copy(
	const KURL & // src
	, const KURL & // dest
	, int // permissions
	, bool // overwrite
	)
{

}

void kio_afpProtocol::del( const KURL & // url
	, bool // isfile 
	)
{
  printf("del\n");
  finished();

}
void kio_afpProtocol::chmod( const KURL & // url
	, int // permissions 
)
{
  printf("chmod\n");
  finished();

}


void kio_afpProtocol::statVolume(void)
{

    UDSEntry entry;
    UDSAtom atom;

    atom.m_uds = KIO::UDS_NAME;
    atom.m_str = QString::null;
    entry.append( atom );

    atom.m_uds = KIO::UDS_FILE_TYPE;
    atom.m_long = S_IFDIR;
    entry.append( atom );

    atom.m_uds = KIO::UDS_ACCESS;
    atom.m_long = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    entry.append( atom );

    atom.m_uds = KIO::UDS_USER;
    atom.m_str = "root";
    entry.append( atom );
    atom.m_uds = KIO::UDS_GROUP;
    entry.append( atom );

    // no size
    statEntry( entry );
}

void kio_afpProtocol::statServer(void)
{
    UDSEntry entry;
    UDSAtom atom;

    atom.m_uds = KIO::UDS_NAME;
    atom.m_str = QString::null;
    entry.append( atom );

    atom.m_uds = KIO::UDS_FILE_TYPE;
    atom.m_long = S_IFDIR;
    entry.append( atom );

    atom.m_uds = KIO::UDS_ACCESS;
    atom.m_long = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    entry.append( atom );

    atom.m_uds = KIO::UDS_USER;
    atom.m_str = "root";
    entry.append( atom );
    atom.m_uds = KIO::UDS_GROUP;
    entry.append( atom );

    // no size
    statEntry( entry );
}


UDSEntry kio_afpProtocol::FPToEntry(struct afp_file_info_basic * fp)
{
    UDSEntry entry;
    UDSAtom atom;

    atom.m_uds = UDS_NAME;
    atom.m_str = fp->name;
    entry.append( atom );

      if (S_ISDIR(fp->unixprivs.permissions)) {
         atom.m_uds = UDS_GUESSED_MIME_TYPE;
         atom.m_str = "inode/directory";
         entry.append( atom );

         atom.m_uds = UDS_FILE_TYPE;
         atom.m_long = S_IFDIR;
         entry.append( atom );
      } else {
         atom.m_uds = UDS_GUESSED_MIME_TYPE;
         atom.m_str = "text/plain";
         entry.append( atom );

         atom.m_uds = UDS_FILE_TYPE;
         atom.m_long = S_IFREG;
         entry.append( atom );
      }

      atom.m_uds = UDS_SIZE;
      atom.m_long = fp->size;
      entry.append( atom );

      atom.m_uds = UDS_MODIFICATION_TIME;
      atom.m_long = fp->modification_date;
      entry.append( atom );

      atom.m_uds = UDS_ACCESS;
    //  atom.m_long = fp->unixprivs.permissions;
    atom.m_long = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
      entry.append( atom );

      atom.m_uds = UDS_USER;
      atom.m_str = fp->unixprivs.uid;
      entry.append( atom );

    atom.m_uds = KIO::UDS_USER;
    struct passwd * passwd;
    if ((passwd = getpwuid(fp->unixprivs.uid))) {
        atom.m_str = passwd->pw_name;;
    } else {
        atom.m_str = "unknown";
    }
    entry.append( atom );
    atom.m_uds = KIO::UDS_GROUP;
    entry.append( atom );

    return entry;
}

UDSEntry kio_afpProtocol::statToEntry(KDE_struct_stat* stat)
{
    UDSEntry entry;
    UDSAtom atom;

      if (S_ISDIR(stat->st_mode)) {
         atom.m_uds = UDS_GUESSED_MIME_TYPE;
         atom.m_str = "inode/directory";
         entry.append( atom );

         atom.m_uds = UDS_FILE_TYPE;
         atom.m_long = S_IFDIR;
         entry.append( atom );
      } else {
         atom.m_uds = UDS_GUESSED_MIME_TYPE;
         atom.m_str = "text/plain";
         entry.append( atom );

         atom.m_uds = UDS_FILE_TYPE;
         atom.m_long = S_IFREG;
         entry.append( atom );
      }

    atom.m_uds = KIO::UDS_ACCESS;
    atom.m_long = stat->st_mode;
    entry.append( atom );

#if 0
    atom.m_uds = KIO::UDS_USER;
    struct passwd * passwd;
    if ((passwd = getpwuid(stat->st_uid))) {
        atom.m_str = passwd->pw_name;;
    } else {
        atom.m_str = "unknown";
    }
    entry.append( atom );
    atom.m_uds = KIO::UDS_GROUP;
    entry.append( atom );
#endif

    return entry;
}

#if 0
int kio_afpProtocol::checkVolPassword(void)
{


}
#endif


void kio_afpProtocol::stat(const KURL &url)
{

    kdDebug(7101) << "\n\nstat " << url.prettyURL() << endl;
    struct afp_url afp_url;
    QString kafpurl = url.url(0,0);
    afp_default_url(&afp_url);
    afp_parse_url(&afp_url,kafpurl,0);

    if (afpLogin(&afp_url)) {
       kdDebug(7101) << "*** Cannot login, so can't stat" << endl ;
       finished();
       return;
    }

    if (url.path().isEmpty()) {
kdDebug(7101) << "Just a server" << endl;
       statServer();
       finished();
       return;
    }

    afpURL afpurl(url);

    if (afpurl.afp_path().isEmpty()) {
       // We have just a volume
kdDebug(7101) << "Just a volume" << endl;
       statVolume();
       finished();
       return;
    }

    if (attachvolume(&afp_url)==false) {
    kdDebug(7101) << "could not attach in stat " << url.prettyURL() << endl;
       finished();
       return;
    }

    struct stat stat;
    int ret;

    ret = afp_sl_stat(NULL,NULL,&afp_url,&stat);

    UDSEntry entry;
// = statToEntry(&stat);
    UDSAtom atom;

    atom.m_uds = KIO::UDS_NAME;
    atom.m_str = url.fileName();
    entry.append( atom );

// Added
    atom.m_uds = KIO::UDS_FILE_TYPE;
    atom.m_long = S_IFDIR;
    entry.append( atom );

    atom.m_uds = KIO::UDS_ACCESS;
    atom.m_long = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    entry.append( atom );

    atom.m_uds = KIO::UDS_USER;
    atom.m_str = "root";
    entry.append( atom );
    atom.m_str = "root";
    atom.m_uds = KIO::UDS_GROUP;
    entry.append( atom );


    statEntry( entry );
    finished();
    return;
}


void kio_afpProtocol::mimetype(const KURL & url)
{
    kdDebug(7101) << "*** mimetype for " << url.prettyURL() << endl;
    // mimeType("text/plain");
    mimeType("inode/directory");
    finished();
}

extern "C"
{
    int kdemain(int argc, char **argv)
    {
	printf("new kio_afp");
        KInstance instance( "kio_afp" );
        
        if (argc != 4) {
            kdDebug(7101) << "Usage: kio_afp  protocol domain-socket1 domain-socket2" << endl;
            exit(-1);
        }

	// KApplication is necessary to use other ioslaves
	putenv(strdup("SESSION_MANAGER="));

	KApplication::disableAutoDcopRegistration();

	KApplication app(argc, argv, "kio_afp", false, true);

        kio_afpProtocol slave("kio_afp",argv[2], argv[3]);
        slave.dispatchLoop();
        
        kdDebug(7101) << "*** kio_afp Done" << endl;
        return 0;
    }
} 
