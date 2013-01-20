
#ifndef _kafp_H_
#define _kafp_H_

#include <qstring.h>
#include <qcstring.h>
#include <qdir.h>

#include <kurl.h>
#include <kio/global.h>
#include <kio/slavebase.h>
#include <kio/file.h>

#include <afp.h>
#include <kmainwindow.h>
#include <qobject.h>

#include <afpsl.h>

#include <afploginwidgetbase.h>

#include <kde_file.h>

using namespace KIO;

#define KIO_NUM_VOLS 10

class QCString;

class afpURL 
{
public:
    afpURL(const KURL &url);
    QString afp_path(void);
    QString volumename(void);
private:
    QString p_volumename;
    QString p_shortpath;
};


class kio_afpProtocol : public QObject, public KIO::SlaveBase
{
    Q_OBJECT

public:

    kio_afpProtocol(const QCString &protocol,
                        const QCString &poolSocket,
                        const QCString &appSocket);
    ~kio_afpProtocol();

	void update_logmessage(QString new_message);
	void clear_logmessage(void);


    virtual void mimetype(const KURL& url);
    virtual void get(const KURL& url);
	virtual void put(const KURL &url, int permissions,
	bool overwrite, bool resume);

    virtual void listDir( const KURL & url );
    virtual void mkdir( const KURL & url, int permissions );
    virtual void rename( const KURL & src, const KURL & dest, bool overwrite );
    virtual void del( const KURL & url, bool isfile );
    virtual void chmod( const KURL & url, int permissions );
    virtual void stat( const KURL &url );
    virtual void slave_status(void);
	virtual void symlink(const QString &target, const KURL &dest,
	bool overwrite);
	virtual void copy(const KURL &src, const KURL &dest,
	int permissions, bool overwrite);

private:
    bool handle_connect_errors(int ret);
    int afpLogin( struct afp_url * afp_url);
    int listRealDir( const KURL & url );
    bool listVolumes(const KURL & URL);
    void statServer(void);
    void statVolume(void);
    bool attachvolume( struct afp_url *);
    void showVolumes(struct afp_url *);

	bool logged_into_server(struct afp_url * afp_url);
	bool attached_to_volume(struct afp_url * afp_url);

    UDSEntry statToEntry(KDE_struct_stat * stat);
    UDSEntry FPToEntry(struct afp_file_info_basic * fp);
    bool prepare_mountpoint(const char * t);
    QString translated_path(const QString gotpath);


    QDir mountpoint;
    bool m_url_parsed;
    bool m_bLoggedOn;
    bool m_bAttached;
    char m_servername[AFP_SERVER_NAME_LEN];
    char m_volumename[AFP_VOLUME_NAME_LEN];
    bool m_mountpointPrepared;
    volumeid_t volumeid;
    QString status_line_text;

    struct afp_url m_url;

    afploginWidgetBase  *dlg;
struct afp_volume_summary volumes_on_server[KIO_NUM_VOLS];


protected slots:
	void volume_list_attach(QListBoxItem *);
    void do_connect(void);
	void do_attach(void);

};

#endif
