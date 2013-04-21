/*
 *  BoxIt - Manjaro Linux Repository Management Software
 *  Roland Singer <roland@manjaro.org>
 *
 *  Copyright (C) 2007 Free Software Foundation, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QCoreApplication>
#include <iostream>
#include <iomanip>
#include <string>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QList>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include "boxitsocket.h"
#include "const.h"
#include "version.h"
#include "timeoutreset.h"
#include "interactiveprocess.h"

using namespace std;



enum ARGUMENTS {
    ARG_NONE = 0x0000,
    ARG_PULL = 0x0001,
    ARG_PUSH = 0x0002,
    ARG_INIT = 0x0004,
    ARG_PASSWD = 0x0008,
    ARG_STATE = 0x0010,
    ARG_ERRORS = 0x0020,
    ARG_SYNC = 0x0040,
    ARG_SNAP = 0x0080
};



struct Repo {
    QString path, name, architecture, state;
    bool isSyncRepo;
    QStringList overlayPackages, syncPackages;
};



struct Branch {
    QString path, configPath, serverURL, name;
    QList<Repo> repos;
};



struct LocalRepo {
    Repo *parentRepo;
    QString path, name, architecture;
    QStringList packages, signatures, addPackages, removePackages;
};





// General
void catchSignal(int);
void interruptEditorProcess(int sig);
void printHelp();
void drawLogo();
bool fixFilePermission();
bool rmDir(QString path, bool onlyHidden = false, bool onlyContent = false);
QString getNameofPKG(QString pkg);
Version getVersionofPKG(QString pkg);
QString getArchofPKG(QString pkg);
QByteArray sha1CheckSum(QString filePath);

// Input
void consoleEchoMode(bool on = true);
void consoleFill(char fill, int wide);
QString getInput(QString beg = "", bool hide = false, bool addToHistory = true);

// Readline
static char** completion(const char*, int ,int);
char* generator(const char*,int);
char * dupstr (char*);
void * xmalloc (int);

// General socket operations
bool connectAndLoginToHost(QString host);
bool connectIfRequired(QString & host);
void resetServerTimeout();

// Branch & Repo operations
bool createBranch();
bool initBranch(const QString path);
bool initRepo(Repo *repo);
bool readPackagesConfig(const QString path, QStringList & packages);
bool writePackagesConfig(const QString path, const QStringList & packages);
bool saveBranchConfigs();
bool getAllRemoteBranches(QStringList & branches);
bool fillBranchRepos(Branch & branch, bool withPackages);

bool pullBranch();
bool applySymlinks(const QStringList & packages, const QString path, const QString link, const QString coutPath, bool & changedFiles);

bool pushBranch();
bool checkValidArchitecture(QList<LocalRepo> & repos);
bool checkDuplicatePackages(QList<LocalRepo> & repos);
bool checkSignatures(QList<LocalRepo> & repos);
bool checkMatchRepositories(QList<LocalRepo> & repos);
bool checkOverwriteSyncPackages(QList<LocalRepo> & repos);
bool uploadData(const QString path, const int currentIndex, const int maxIndex);

bool listenOnStatus(bool exitOnSessionEnd);
bool printErrors();

bool syncBranch();
bool changeSyncUrlOnRequest(const QString branchName);
bool changeSyncExcludeFiles(const QString branchName);

bool changePassword();

bool snapshotBranch();


// Auto completion
const char* cmd[] = { "www.repo.manjaro.org", "manjaro.org", "repo.manjaro.org" };


// Variables
BoxitSocket boxitSocket;
ARGUMENTS arguments;
quint16 msgID;
QByteArray data;
Branch branch;
QString username;
InteractiveProcess *editorProcess = NULL;




int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    editorProcess = new InteractiveProcess(&app);

    // Set readline completion
    rl_attempted_completion_function = completion;

    //enable auto-complete
    rl_bind_key('\t', rl_complete);


    // Setup signals
    signal(SIGHUP, catchSignal);
    signal(SIGINT, catchSignal);
    signal(SIGTERM, catchSignal);
    signal(SIGKILL, catchSignal);


    // Get command line arguments
    arguments = ARG_NONE;

    for (int nArg=1; nArg < argc; nArg++) {
        if (strcmp(argv[nArg], "help") == 0) {
            printHelp();
            return 0;
        }
        else if (strcmp(argv[nArg], "passwd") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_PASSWD);
        }
        else if (strcmp(argv[nArg], "pull") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_PULL);
        }
        else if (strcmp(argv[nArg], "push") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_PUSH);
        }
        else if (strcmp(argv[nArg], "init") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_INIT);
        }
        else if (strcmp(argv[nArg], "state") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_STATE);
        }
        else if (strcmp(argv[nArg], "errors") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_ERRORS);
        }
        else if (strcmp(argv[nArg], "sync") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_SYNC);
        }
        else if (strcmp(argv[nArg], "snap") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_SNAP);
        }
        else {
            cerr << "invalid option: " << argv[nArg] << endl << endl;
            printHelp();
            return 1;
        }
    }

    if (arguments == ARG_NONE) {
        printHelp();
        return 1;
    }

    // Basic checks
    if ((arguments & ARG_PULL && arguments & ARG_PUSH) || (arguments & ARG_PULL && arguments & ARG_INIT) || (arguments & ARG_PUSH && arguments & ARG_INIT)) {
        cerr << "error: cannot push, pull or init at once!" << endl;
        return 1;
    }

    // Sync argument
    if (arguments & ARG_SYNC) {
        if (syncBranch())   return 0;
        else                return 1; // Error messages are printed by the method
    }

    // Snap argument
    if (arguments & ARG_SNAP) {
        if (snapshotBranch())   return 0;
        else                return 1; // Error messages are printed by the method
    }

    // Passwd argument
    if (arguments & ARG_PASSWD) {
        if (changePassword())  return 0;
        else                return 1; // Error messages are printed by the method
    }

    // State argument
    if (arguments & ARG_STATE) {
        if (listenOnStatus(false))  return 0;
        else                        return 1; // Error messages are printed by the method
    }

    // Error argument
    if (arguments & ARG_ERRORS) {
        if (printErrors())  return 0;
        else                return 1; // Error messages are printed by the method
    }

    // Init argument
    if (arguments & ARG_INIT) {
        if (createBranch()) return 0;
        else                return 1; // Error messages are printed by the method
    }

    // Init and read config of branch
    if (!initBranch(QDir::currentPath()))
        return 1; // Error messages are printed by the method

    // Pull argument
    if (arguments & ARG_PULL && !pullBranch())
        return 1; // Error messages are printed by the method

    // Push argument
    if (arguments & ARG_PUSH && !pushBranch())
        return 1; // Error messages are printed by the method


    return 0;
}



void catchSignal(int) {
    if (boxitSocket.state() == QAbstractSocket::ConnectedState)
        boxitSocket.disconnectFromHost();

    consoleEchoMode(true);

    exit(1);
}



void interruptEditorProcess(int sig) {
    signal(sig, SIG_DFL);

    if (editorProcess)
        editorProcess->kill();
}



void printHelp() {
    drawLogo();
    cout << "Roland Singer <roland@manjaro.org>" << endl << endl;

    cout << "Usage: boxit [OPTIONS] [...]" << endl << endl;
    cout << "  help\t\tshow help" << endl;
    cout << "  init\t\tinitiate branch and pull latest source" << endl;
    cout << "  pull\t\tupdate an existing branch" << endl;
    cout << "  push\t\tupload all changes" << endl;
    cout << "  sync\t\tsynchronize branch" << endl;
    cout << "  snap\t\tsnapshot branch" << endl;
    cout << "  state\t\tshow state" << endl;
    cout << "  errors\tshow all remote errors" << endl;
    cout << "  passwd\tchange user password" << endl;
    cout << endl;
}



void drawLogo() {
    cout << " ____  _____  _  _  ____  ____ " << endl;
    cout << "(  _ \\(  _  )( \\/ )(_  _)(_  _)" << endl;
    cout << " ) _ < )(_)(  )  (  _)(_   )(  " << endl;
    cout << "(____/(_____)(_/\\_)(____) (__) " << endl << endl;
}



bool fixFilePermission(QString file) {
    int ret;

    // Set right permission
    mode_t process_mask = umask(0);
    ret = chmod(file.toUtf8().data(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH);
    umask(process_mask);

    return (ret == 0);
}



bool rmDir(QString path, bool onlyHidden, bool onlyContent) {
    if (!QDir().exists(path))
        return true;

    bool success = true;

    // Remove content of dir
    QStringList list = QDir(path).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);

    for (int i = 0; i < list.size(); ++i) {
        if (onlyHidden && !list.at(i).trimmed().startsWith("."))
            continue;

        QFileInfo info(path + "/" + list.at(i));

        if (info.isDir()) {
            if (!rmDir(path + "/" + list.at(i)), onlyHidden)
                success = false;
        }
        else {
            if (!QFile::remove(path + "/" + list.at(i)))
                success = false;
        }
    }

    if (!onlyContent && !QDir().rmdir(path))
        return false;

    return success;
}



QString getNameofPKG(QString pkg) {
    QString work;
    pkg = pkg.split("/", QString::SkipEmptyParts).last();
    work = pkg.section("-", -3, -1);

    return QString(pkg).remove(pkg.size() - (work.size() + 1), work.size() + 1).trimmed();
}



Version getVersionofPKG(QString pkg) {
    pkg = pkg.split("/", QString::SkipEmptyParts).last();
    pkg = pkg.section("-", -3, -1);
    pkg.remove(pkg.lastIndexOf("-"), pkg.size());

    return Version(pkg);
}



QString getArchofPKG(QString pkg) {
    pkg = pkg.split("/", QString::SkipEmptyParts).last();
    pkg = pkg.section("-", -1, -1);

    return pkg.section(".", 0, 0);
}



QByteArray sha1CheckSum(QString filePath) {
    QCryptographicHash crypto(QCryptographicHash::Sha1);
    QFile file(filePath);

    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    while(!file.atEnd()){
        crypto.addData(file.read(8192));
    }

    return crypto.result();
}



//###
//### Input
//###



void consoleEchoMode(bool on) {
    struct termios settings;
    tcgetattr( STDIN_FILENO, &settings );
    settings.c_lflag = on
                  ? (settings.c_lflag |   ECHO )
                  : (settings.c_lflag & ~(ECHO));
    tcsetattr( STDIN_FILENO, TCSANOW, &settings );
}



void consoleFill(char fill, int wide) {
    cout << setfill(fill) << setw(wide) << fill << endl;
    cout << setfill(' ');
}



QString getInput(QString beg, bool hide, bool addToHistory) {
    QString str;

    if (hide)
        consoleEchoMode(false);

    str = QString(readline(beg.toUtf8().data()));

    if (hide) {
        consoleEchoMode(true);
        cout << endl;
    }

    if (str.contains(BOXIT_SPLIT_CHAR)) {
        cerr << "Your input contains an invalid character: '" << BOXIT_SPLIT_CHAR << "'" << endl;
        return getInput(beg, hide);
    }

    if (!str.isEmpty() && addToHistory)
        add_history(str.trimmed().toUtf8().data());

    return str;
}



//###
//### Readline
//###



static char** completion( const char * text , int start, int)
{
    char **matches;

    matches = (char **)NULL;

    if (start == 0)
        matches = rl_completion_matches ((char*)text, &generator);
    else
        rl_bind_key('\t',rl_abort);

    return (matches);

}



char* generator(const char* text, int state)
{
    static int list_index, len;
    char *name;

    if (!state) {
        list_index = 0;
        len = strlen (text);
    }

    while ((name = (char*)cmd[list_index])) {
        list_index++;

        if (strncmp (name, text, len) == 0)
            return (dupstr(name));
    }

    /* If no names matched, then return NULL. */
    return ((char *)NULL);

}



char * dupstr (char* s) {
  char *r;

  r = (char*) xmalloc ((strlen (s) + 1));
  strcpy (r, s);
  return (r);
}



void * xmalloc (int size)
{
    void *buf;

    buf = malloc (size);
    if (!buf) {
        fprintf (stderr, "Error: Out of memory. Exiting.'n");
        exit (1);
    }

    return buf;
}



//###
//### General socket operations
//###


bool connectAndLoginToHost(QString host) {
    cout << "\r" << flush;
    cout << QString(":: Connecting to host %1...").arg(host.trimmed()).toUtf8().data() << flush;

    // Connect to host
    if (!boxitSocket.connectToHost(host))
        return false; // Error messages are printed by the boxit socket class

    cout << "\r" << flush;
    cout << QString(":: Connected to host %1     ").arg(host.trimmed()).toUtf8().data() << endl;


    // First send our boxit version to the server
    boxitSocket.sendData(MSG_CHECK_VERSION, QByteArray(QString::number((int)BOXIT_VERSION).toUtf8()));
    boxitSocket.readData(msgID);
    if (msgID != MSG_SUCCESS) {
        cerr << "error: the BoxIt server is running a different version! Abording..." << endl;
        goto error;
    }


    // Let's login now
    for (int i=0;; i++) {
        username = getInput(" username: ", false, false);
        QString password = QString(QCryptographicHash::hash(getInput(" password: ", true, false).toLocal8Bit(), QCryptographicHash::Sha1).toHex());

        boxitSocket.sendData(MSG_AUTHENTICATE, QByteArray(QString(username + BOXIT_SPLIT_CHAR + password).toUtf8()));
        boxitSocket.readData(msgID);

        if (msgID == MSG_SUCCESS) {
            break;
        }
        else {
            cerr << "error: failed to login!" << endl;
            if (i>1)
                goto error;
        }
    }


    return true;

error:
    // Disconnect from server
    boxitSocket.disconnectFromHost();
    return false;
}



bool connectIfRequired(QString & host) {
    if (boxitSocket.state() == QAbstractSocket::ConnectedState)
        return true;

    if (host.isEmpty())
        host = getInput(" host: ", false, false).trimmed();

    return connectAndLoginToHost(host);
}



void resetServerTimeout() {
    if (boxitSocket.state() == QAbstractSocket::ConnectedState)
        boxitSocket.sendData(MSG_RESET_TIMEOUT);
}




//###
//### Branch operations
//###



bool createBranch() {
    QString host;

    if (!connectIfRequired(host))
        return false; // Error messages are printed by the method

    // List all available branches
    QStringList branches;

    if (!getAllRemoteBranches(branches))
        return false; // Error messages are printed by the method

    cout << ":: Available branches:" << endl << endl;
    for (int i = 0; i < branches.size(); ++i) {
        cout << " " << QString::number(i + 1).toUtf8().data() << ") " << branches.at(i).toUtf8().data() << endl;
    }
    cout << endl;


    // Get user input
    int index;

    while (true) {
        index = getInput(":: Branch index: ", false, false).trimmed().toInt();

        if (index <= 0 || index > branches.size()) {
            cerr << "error: index is invalid!" << endl;
            continue;
        }

        break;
    }
    --index;

    cout << ":: Initialising branch directory '" << branches.at(index).toUtf8().data() << "'" << endl;

    // Create directory
    QString path = QDir::currentPath() + "/" + branches.at(index);

    if (QDir(path).exists()) {
        cerr << "error: directory '" << path.toUtf8().data() << "' already exists!" << endl;
        return false;
    }

    if (!QDir(path).mkdir(path)) {
        cerr << "error: failed to create directory '" << path.toUtf8().data() << "'!" << endl;
        return false;
    }

    // Change current working directory
    if (!QDir::setCurrent(path)) {
        cerr << "error: failed to change current working directory!" << endl;
        return false;
    }

    // Setup branch struct
    branch.name = branches.at(index);
    branch.serverURL = host;
    branch.path = QDir::currentPath();
    branch.configPath = branch.path + "/" + BOXIT_DIR;

    // Pull branch
    if (!pullBranch())
        return false; // Error messages are printed by the method

    return true;
}



bool initBranch(const QString path) {
    branch.repos.clear();
    branch.path = path;
    branch.configPath = path + "/" + BOXIT_DIR;

    QString configPath = branch.configPath + "/" + BOXIT_CONFIG;

    // Check if .boxit/config exists
    if (!QFile::exists(configPath)) {
        cerr << "error: config file '" << configPath.toUtf8().data() << "' does not exists!" << endl;
        return false;
    }

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cerr << "error: failed to read config file '" << configPath.toUtf8().data() << "'!" << endl;
        return false;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().split("#", QString::KeepEmptyParts).first().trimmed();
        if (line.isEmpty() || !line.contains("="))
            continue;

        QString arg1 = line.split("=").first().toLower().trimmed();
        QString arg2 = line.split("=").last().trimmed();

        if (arg1 == "server") {
            branch.serverURL = arg2;
        }
        else if (arg1 == "branch") {
            branch.name = arg2;
        }
        else if (arg1 == "repository") {
            QStringList split = arg2.split(":", QString::SkipEmptyParts);
            if (split.size() < 2) {
                cerr << "error: branch config contains an invalid repo variable!" << endl;
                return false;
            }

            Repo repo;
            repo.name = split.at(0).trimmed();
            repo.architecture = split.at(1).trimmed();
            repo.path = branch.path + "/" + repo.name + "/" + repo.architecture;
            repo.isSyncRepo = false;

            branch.repos.append(repo);
        }
    }

    file.close();

    if (branch.name.isEmpty() || branch.serverURL.isEmpty()) {
        cerr << "error: branch config '" << configPath.toUtf8().data() << "' is incomplete!" << endl;
        return false;
    }


    // Fill repositories
    for (int i = 0; i < branch.repos.size(); ++i) {
        Repo *repo = &branch.repos[i];

        if (!initRepo(repo)) {
            cerr << "error: failed to init repository " << repo->name.toUtf8().data() << " " << repo->architecture.toUtf8().data() << "!" << endl;
            return false;
        }
    }

    return true;
}



bool initRepo(Repo *repo) {
    QString configPath = branch.configPath + "/" + repo->name + "_" + repo->architecture;

    // Check if repo config exists
    if (!QFile::exists(configPath)) {
        cerr << "error: config file '" << configPath.toUtf8().data() << "' does not exists!" << endl;
        return false;
    }

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cerr << "error: failed to read config file '" << configPath.toUtf8().data() << "'!" << endl;
        return false;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().split("#", QString::KeepEmptyParts).first().trimmed();
        if (line.isEmpty() || !line.contains("="))
            continue;

        QString arg1 = line.split("=").first().toLower().trimmed();
        QString arg2 = line.split("=").last().trimmed();

        if (arg1 == "state")
            repo->state = arg2;
        else if (arg1 == "sync")
            repo->isSyncRepo = (bool)arg2.toInt();
    }

    file.close();

    if (repo->state.isEmpty()) {
        cerr << "error: repository config '" << configPath.toUtf8().data() << "' is incomplete!" << endl;
        return false;
    }


    // Obtain overlay packages
    configPath = branch.configPath + "/" + repo->name + "_" + repo->architecture + "_" + BOXIT_OVERLAY_PACKAGES_CONFIG;
    if (!readPackagesConfig(configPath, repo->overlayPackages)) {
        cerr << "error: failed to read overlay packages config '" << configPath.toUtf8().data() << "'!" << endl;
        return false;
    }

    // Obtain sync packages if this is a sync repository
    configPath = branch.configPath + "/" + repo->name + "_" + repo->architecture + "_" + BOXIT_SYNC_PACKAGES_CONFIG;
    if (repo->isSyncRepo && !readPackagesConfig(configPath, repo->syncPackages)) {
        cerr << "error: failed to read sync packages config '" << configPath.toUtf8().data() << "'!" << endl;
        return false;
    }

    return true;
}



bool readPackagesConfig(const QString path, QStringList & packages) {
    packages.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        packages.append(line);
    }

    file.close();

    return true;
}



bool writePackagesConfig(const QString path, const QStringList & packages) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    foreach (const QString package, packages)
        out << package << "\n";

    file.close();

    return true;
}



bool saveBranchConfigs() {
    // Create config folder is required
    if (!QDir(branch.configPath).exists() && !QDir().mkdir(branch.configPath)) {
        cerr << "error: failed to create directory '" << branch.configPath.toUtf8().data() << "'!" << endl;
        return false;
    }


    // Save branch config
    QString configPath = branch.configPath + "/" + BOXIT_CONFIG;

    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cerr << "error: failed to open file '" << configPath.toUtf8().data() << "'!" << endl;
        return false;
    }

    QTextStream out(&file);
    out << "##\n## BoxIt Branch Configuration\n##\n\n";
    out << "server=" << branch.serverURL << "\n";
    out << "branch=" << branch.name << "\n";
    out << "\n# Branch Repositories\n";

    // Add all repositories to branch config
    for (int i = 0; i < branch.repos.size(); ++i) {
        const Repo *repo = &branch.repos.at(i);
        out << "repository=" << repo->name << ":" << repo->architecture << "\n";
    }

    file.close();


    // Remove all other files
    QStringList files = QDir(branch.configPath).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    foreach (QString file, files) {
        if (file == BOXIT_CONFIG)
            continue;

        if (!QFile::remove(branch.configPath + "/" + file)) {
            cerr << "error: failed to remove file '" << QString(branch.configPath + "/" + file).toUtf8().data() << "'!" << endl;
            return false;
        }
    }


    // Create repository configs
    for (int i = 0; i < branch.repos.size(); ++i) {
        const Repo *repo = &branch.repos.at(i);

        file.setFileName(branch.configPath + "/" + repo->name + "_" + repo->architecture);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            cerr << "error: failed to open file '" << file.fileName().toUtf8().data() << "'!" << endl;
            return false;
        }

        QTextStream out(&file);
        out << "##\n## BoxIt Repository Configuration\n##\n\n";
        out << "state=" << repo->state << "\n";
        out << "sync=" << QString::number((int)repo->isSyncRepo) << "\n";

        file.close();

        // Save overlay package list
        QString configPath = branch.configPath + "/" + repo->name + "_" + repo->architecture + "_" + BOXIT_OVERLAY_PACKAGES_CONFIG;
        if (!writePackagesConfig(configPath, repo->overlayPackages)) {
            cerr << "error: failed to save overlay package list '" << configPath.toUtf8().data() << "'!" << endl;
            return false;
        }

        // Save sync package list
        configPath = branch.configPath + "/" + repo->name + "_" + repo->architecture + "_" + BOXIT_SYNC_PACKAGES_CONFIG;
        if (!writePackagesConfig(configPath, repo->syncPackages)) {
            cerr << "error: failed to save sync package list '" << configPath.toUtf8().data() << "'!" << endl;
            return false;
        }
    }

    // Finally create dummy file
    if (!QFile::copy(":/resources/resources/dummy.tar.xz", branch.configPath + "/" + BOXIT_DUMMY_FILE)) {
        cerr << "error: failed to create dummy file '" << QString(branch.configPath + "/" + BOXIT_DUMMY_FILE).toUtf8().data() << "'!" << endl;
        return false;
    }

    // Fix file permission
    fixFilePermission(branch.configPath + "/" + BOXIT_DUMMY_FILE);

    return true;
}



bool getAllRemoteBranches(QStringList & branches) {
    branches.clear();

    boxitSocket.sendData(MSG_GET_BRANCHES);
    boxitSocket.readData(msgID, data);

    while (msgID == MSG_DATA_BRANCH) {
        branches.append(QString(data));
        boxitSocket.readData(msgID, data);
    }

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to obtain remote branches!" << endl;
        return false;
    }

    return true;
}



bool fillBranchRepos(Branch & branch, bool withPackages) {
    cout << " obtaining remote repository packages..." << endl;

    branch.repos.clear();

    if (withPackages)
        msgID = MSG_GET_REPOS_WITH_PACKAGES;
    else
        msgID = MSG_GET_REPOS;

    boxitSocket.sendData(msgID, QByteArray(branch.name.toUtf8()));
    boxitSocket.readData(msgID, data);

    while (msgID == MSG_DATA_REPO || msgID == MSG_DATA_OVERLAY_PACKAGES || msgID == MSG_DATA_SYNC_PACKAGES) {
        QStringList split = QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);

        // Repository
        if (msgID == MSG_DATA_REPO) {
            if (split.size() < 4) {
                cerr << "warning: invalid server reply: MSG_DATA_REPO" << endl;
                return false;
            }

            Repo repo;
            repo.name = split.at(0);
            repo.architecture = split.at(1);
            repo.state = split.at(2);
            repo.isSyncRepo = (bool)split.at(3).toInt();
            repo.path = branch.path + "/" + repo.name + "/" + repo.architecture;

            branch.repos.append(repo);
        }
        else if (msgID == MSG_DATA_OVERLAY_PACKAGES) {
            if (!branch.repos.isEmpty() && !split.isEmpty())
                branch.repos.last().overlayPackages.append(split);
        }
        else if (msgID == MSG_DATA_SYNC_PACKAGES) {
            if (!branch.repos.isEmpty() && !split.isEmpty())
                branch.repos.last().syncPackages.append(split);
        }

        boxitSocket.readData(msgID, data);
    }

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to obtain remote branch repositories! Does the branch exists?" << endl;
        return false;
    }

    return true;
}



bool pullBranch() {
    // Check if connected...
    if (!connectIfRequired(branch.serverURL))
        return false; // Error messages are printed by the method

    cout << ":: Pulling changes..." << endl;


    // Update branch repositories
    Branch oldBranch = branch;
    bool branchIsUpToDate = true;

    if (!fillBranchRepos(branch, true))
        return false; // Error messages are printed by the method

    // Check if current branch is up-to-date
    for (int i = 0; i < branch.repos.size(); ++i) {
        const Repo *remoteRepo = &branch.repos.at(i);
        bool found = false;

        for (int i = 0; i < oldBranch.repos.size(); ++i) {
            const Repo *repo = &oldBranch.repos.at(i);
            if (repo->name != remoteRepo->name || repo->architecture != remoteRepo->architecture)
                continue;

            if (repo->state != remoteRepo->state) {
                branchIsUpToDate = false;
                break;
            }

            found = true;
            break;
        }

        if (!found) {
            branchIsUpToDate = false;
            break;
        }
    }


    // Save new branch configurations
    if (!saveBranchConfigs())
        return false; // Error messages are printed by the method

    // Remove all folders except the repo folders
    QStringList folders = QDir(branch.path).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (int i = 0; i < branch.repos.size(); ++i) {
        folders.removeAll(branch.repos.at(i).name);
    }

    if (!folders.isEmpty()) {
        QString answer = getInput(":: " + QString::number(folders.size()) + " orphan folder(s) found which will be removed!\n   Continue? [Y/n] ", false, false).toLower().trimmed();
        if (!answer.isEmpty() && answer != "y") {
            cerr << "abording..." << endl;
            return false;
        }

        foreach (QString folder, folders) {
            if (!rmDir(branch.path + "/" + folder)) {
                cerr << "error: failed to remove directory '" << QString(branch.path + "/" + folder).toUtf8().data() << "'!" << endl;
                return false;
            }
        }
    }


    // Create repository directories
    for (int i = 0; i < branch.repos.size(); ++i) {
        const Repo *repo = &branch.repos.at(i);

        if (!QDir(repo->path).exists() && !QDir().mkpath(repo->path)) {
            cerr << "error: failed to create directory '" << repo->path.toUtf8().data() << "'!" << endl;
            return false;
        }

        // Apply Symlinks
        bool changedFiles;
        if (!applySymlinks(repo->overlayPackages,
                           repo->path, "../../" + QString(BOXIT_DIR) + "/" + QString(BOXIT_DUMMY_FILE),
                           repo->name + "/" + repo->architecture,
                           changedFiles))
            return false; // Error messages are printed by the method

        if (changedFiles)
            branchIsUpToDate = false;
    }

    if (branchIsUpToDate)
        cout << ":: Already up-to-date." << endl;
    else
        cout << ":: Branch is up-to-date now." << endl;

    return true;
}



bool applySymlinks(const QStringList & packages, const QString path, const QString link, const QString coutPath, bool & changedFiles) {
    changedFiles = false;

    // List of local files in path
    QStringList localFiles = QDir(path).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

    // Remove old files
    foreach (const QString file, localFiles) {
        if (packages.contains(file))
            continue;

        changedFiles = true;

        QString filePath = path + "/" + file;
        QFileInfo info(filePath);

        cout << "   remove  " << QString(coutPath + "/" + file).toUtf8().data() << endl;

        // Remove file
        if (info.isSymLink() && unlink(filePath.toStdString().c_str()) != 0) {
            cerr << "error: failed to remove symlink '" << filePath.toUtf8().data() << "'!" << endl;
            return false;
        }
        else if (info.isFile() && !QFile::remove(filePath)) {
            cerr << "error: failed to remove file '" << filePath.toUtf8().data() << "'!" << endl;
            return false;
        }
    }

    // Create new symlinks
    foreach (const QString package, packages) {
        QString filePath = path + "/" + package;
        QFileInfo info(filePath);

        if (localFiles.contains(package) && info.isSymLink())
            continue;

        changedFiles = true;

        if (info.isFile()) {
            if (!QFile::remove(filePath)) {
                cerr << "error: failed to remove file '" << filePath.toUtf8().data() << "'!" << endl;
                return false;
            }

            cout << "   replace " << QString(coutPath + "/" + package).toUtf8().data() << endl;
        }
        else {
            cout << "   create  " << QString(coutPath + "/" + package).toUtf8().data() << endl;
        }

        // Symlink package
        if (!QFile::link(link, filePath)) {
            cerr << "error: failed to create symlink '" << filePath.toUtf8().data() << "' to '" << link.toUtf8().data() << "'!" << endl;
            return false;
        }
    }

    return true;
}



bool pushBranch() {
    QList<LocalRepo> localRepos;
    QStringList addFiles, addFilesWithPath, addVirtualPackages, infoAddPackages, infoRemovePackages;
    bool nothingToDo = true;

    // Get packages of all local repositories
    for (int i = 0; i < branch.repos.size(); ++i) {
        Repo *repo = &branch.repos[i];
        LocalRepo localRepo;

        localRepo.parentRepo = repo;
        localRepo.name = repo->name;
        localRepo.architecture = repo->architecture;
        localRepo.path = repo->path;
        localRepo.packages = QDir(localRepo.path).entryList(QString(BOXIT_PACKAGE_FILTERS).split(" ", QString::SkipEmptyParts), QDir::Files | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
        localRepo.signatures = QDir(localRepo.path).entryList(QStringList() << "*" + QString(BOXIT_SIGNATURE_ENDING), QDir::Files | QDir::System | QDir::NoDotAndDotDot, QDir::Name);

        localRepos.append(localRepo);
    }

    // Check for invalid architectures...
    if (!checkValidArchitecture(localRepos))
        return false; // Error messages are printed by the method

    // Check for duplicate packages...
    if (!checkDuplicatePackages(localRepos))
        return false; // Error messages are printed by the method

    // Check signatures...
    if (!checkSignatures(localRepos))
        return false; // Error messages are printed by the method

    // Check if subrepositories have different packages...
    if (!checkMatchRepositories(localRepos))
        return false; // Error messages are printed by the method

    // Check if sync packages are overwritten...
    if (!checkOverwriteSyncPackages(localRepos))
        return false; // Error messages are printed by the method


    // Get all packages to add and to remove...
    for (int i = 0; i < localRepos.size(); ++i) {
        LocalRepo *localRepo = &localRepos[i];
        QStringList missingSignatures;

        // Get all packages to remove
        foreach (const QString package, localRepo->parentRepo->overlayPackages) {
            if (localRepo->packages.contains(package))
                continue;

            nothingToDo = false;
            localRepo->removePackages.append(package);
            infoRemovePackages.append(package);
        }

        // Get all packages to add
        foreach (const QString package, localRepo->packages) {
            if (localRepo->parentRepo->overlayPackages.contains(package))
                continue;

            nothingToDo = false;
            localRepo->addPackages.append(package);
            infoAddPackages.append(package);

            QFileInfo info(localRepo->path + "/" + package);

            if (info.isSymLink()) {
                addVirtualPackages.append(package);
            }
            else {
                addFiles.append(package);
                addFiles.append(package + BOXIT_SIGNATURE_ENDING);
                addFilesWithPath.append(localRepo->path + "/" + package);
                addFilesWithPath.append(localRepo->path + "/" + package + BOXIT_SIGNATURE_ENDING);

                // Find all packages with missing signatures if this is a normal file
                if (!localRepo->signatures.contains(package + BOXIT_SIGNATURE_ENDING))
                    missingSignatures.append(package);
            }
        }

        if (missingSignatures.isEmpty())
            continue;

        cerr << QString::number(missingSignatures.size()).toUtf8().data();
        cerr << ":: Package(s) without signature in repository " << localRepo->name.toUtf8().data() << " " << localRepo->architecture.toUtf8().data() << "!" << endl;

        cerr << endl << "packages without signatures:" << endl << endl;

        for (int i = 0; i < missingSignatures.size(); ++i) {
            cerr << "   " << missingSignatures.at(i).toUtf8().data() << endl;
        }

        cerr << endl << "abording..." << endl;

        return false;
    }


    // Inform the user
    if (nothingToDo) {
        cerr << ":: Nothing to do." << endl;
        return false;
    }

    while (true) {
        QString answer = getInput(":: Remove " + QString::number(infoRemovePackages.size()) + " remote package(s) and upload " + QString::number(infoAddPackages.size()) + " package(s) ? [Y/n/d] (d=details) ", false, false).toLower().trimmed();
        if (answer == "d") {
            if (!infoRemovePackages.isEmpty()) {
                cout << endl << "remote packages to remove:" << endl << endl;

                for (int i = 0; i < infoRemovePackages.size(); ++i) {
                    cout << "   " << infoRemovePackages.at(i).toUtf8().data() << endl;
                }
            }

            if (!infoAddPackages.isEmpty()) {
                cout << endl << "packages to upload:" << endl << endl;

                for (int i = 0; i < infoAddPackages.size(); ++i) {
                    cout << "   " << infoAddPackages.at(i).toUtf8().data() << endl;
                }
            }

            cout << endl;
            continue;
        }
        else if (!answer.isEmpty() && answer != "y") {
            cerr << "abording..." << endl;
            return false;
        }
        else {
            break;
        }
    }



    // Check if connected...
    if (!connectIfRequired(branch.serverURL))
        return false; // Error messages are printed by the method


    cout << ":: Pushing changes..." << endl;


    // Get up-to-date branch struct
    Branch remoteBranch = branch;
    if (!fillBranchRepos(remoteBranch, true))
        return false; // Error messages are printed by the method

    // Check if current branch is up-to-date
    for (int i = 0; i < remoteBranch.repos.size(); ++i) {
        const Repo *remoteRepo = &remoteBranch.repos.at(i);
        bool found = false;

        for (int i = 0; i < branch.repos.size(); ++i) {
            const Repo *repo = &branch.repos.at(i);
            if (repo->name != remoteRepo->name || repo->architecture != remoteRepo->architecture)
                continue;

            if (repo->state != remoteRepo->state) {
                cerr << "error: current branch is not up-to-date! Perform a pull operation first!" << endl;
                return false;
            }

            found = true;
            break;
        }

        if (!found) {
            cerr << "error: current branch is not up-to-date! Perform a pull operation first!" << endl;
            return false;
        }
    }




    // Check if virtual packages exists on server
    boxitSocket.sendData(MSG_POOL_CHECK_FILES_EXISTS, QByteArray(addVirtualPackages.join(BOXIT_SPLIT_CHAR).toUtf8()));
    boxitSocket.readData(msgID, data);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: symlinked new packages don't exist on server!" << endl;
        cerr << "missing packages: " << QString(data).replace(BOXIT_SPLIT_CHAR, " ").toUtf8().data() << endl;
        return false;
    }


    // Lock repositories
    for (int i = 0; i < localRepos.size(); ++i) {
        LocalRepo *localRepo = &localRepos[i];

        // Skip repo if there are no changes made
        if (localRepo->addPackages.isEmpty() && localRepo->removePackages.isEmpty())
            continue;

        QStringList list;
        list << branch.name << localRepo->name << localRepo->architecture;

        boxitSocket.sendData(MSG_LOCK_REPO, QByteArray(list.join(BOXIT_SPLIT_CHAR).toUtf8()));
        boxitSocket.readData(msgID);

        if (msgID != MSG_SUCCESS) {
            cerr << "error: failed to lock repository " << localRepo->name.toUtf8().data() << " " << localRepo->architecture.toUtf8().data();
            cerr << " on server! It might be locked by another user or a repository process is currently running..." << endl;
            return false;
        }
    }


    // Lock pool files
    boxitSocket.sendData(MSG_LOCK_POOL_FILES, QByteArray(addFiles.join(BOXIT_SPLIT_CHAR).toUtf8()));
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to lock pool files on server! The files might be locked by another user..." << endl;
        return false;
    }


    // Upload files
    for (int i = 0; i < addFilesWithPath.size(); ++i) {
        if (!uploadData(addFilesWithPath.at(i), i + 1, addFilesWithPath.size()))
            return false;
    }

    if (!addFiles.isEmpty()) {
        cout << "\r                                                                               " << "\r" << flush;
        cout << ":: Finished package upload." << endl;
    }


    // Move new files to pool
    boxitSocket.sendData(MSG_MOVE_POOL_FILES);
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to move uploaded files to pool!" << endl;
        return false;
    }


    // Release pool lock
    boxitSocket.sendData(MSG_RELEASE_POOL_LOCK);
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to release pool lock!" << endl;
        return false;
    }


    // Apply repository changes
    for (int i = 0; i < localRepos.size(); ++i) {
        LocalRepo *localRepo = &localRepos[i];

        // Skip repo if there are no changes made
        if (localRepo->addPackages.isEmpty() && localRepo->removePackages.isEmpty())
            continue;

        QString sendData = branch.name + BOXIT_SPLIT_CHAR + localRepo->name + BOXIT_SPLIT_CHAR + localRepo->architecture;
        sendData += "\n" + localRepo->addPackages.join(BOXIT_SPLIT_CHAR);
        sendData += "\n" + localRepo->removePackages.join(BOXIT_SPLIT_CHAR);

        boxitSocket.sendData(MSG_APPLY_REPO_CHANGES, QByteArray(sendData.toUtf8()));
        boxitSocket.readData(msgID);

        if (msgID != MSG_SUCCESS) {
            cerr << "error: failed to apply changes to repository " << localRepo->name.toUtf8().data() << " " << localRepo->architecture.toUtf8().data() << "!" << endl;
            return false;
        }
    }


    // Release repo lock
    boxitSocket.sendData(MSG_RELEASE_REPO_LOCK);
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to release repository lock!" << endl;
        return false;
    }

    cout << ":: Finished push." << endl;

    // Wait until process session finished
    if (!listenOnStatus(true)) {
        cout << endl << ":: Process session failed! Check the process errors..." << endl;
        return false; // Error messages are printed by the method
    }

    cout << endl << ":: Process session successfully finished." << endl;
    sleep(1);

    // Pull changes
    if (!pullBranch())
        return false; // Error messages are printed by the method

    return true;
}



bool checkValidArchitecture(QList<LocalRepo> & repos) {
    const QStringList skipArchs = QString(BOXIT_SKIP_ARCHITECTURE_CHECKS).split(" ", QString::SkipEmptyParts);

    for (int i = 0; i < repos.size(); ++i) {
        LocalRepo *repo = &repos[i];
        QStringList invalidFiles;

        foreach (const QString package, repo->packages) {
            QString arch = getArchofPKG(package);

            if (!skipArchs.contains(arch) && arch != repo->architecture)
                invalidFiles.append(package);
        }

        if (invalidFiles.isEmpty())
            continue;

        // Inform the user about invalid packages
        while (true) {
            QString answer = getInput(QString(":: %1 package(s) with invalid architecture found in repository %2 %3!\n   Remove them? [Y/n/d] (d=details) ").arg(QString::number(invalidFiles.size()), repo->name, repo->architecture), false, false).toLower().trimmed();
            if (answer == "d") {
                cout << endl << "packages to remove:" << endl << endl;

                for (int i = 0; i < invalidFiles.size(); ++i) {
                    cout << "   " << invalidFiles.at(i).toUtf8().data() << endl;
                }

                cout << endl;
                continue;
            }
            else if (!answer.isEmpty() && answer != "y") {
                cerr << "abording..." << endl;
                return false;
            }
            else {
                // Remove invalid packages
                foreach (const QString file, invalidFiles) {
                    if (!QFile::remove(repo->path + "/" + file)) {
                        cerr << "error: failed to remove file '" << file.toUtf8().data() << "'!" << endl;
                        return false;
                    }

                    // Also remove package out of packages list
                    repo->packages.removeAll(file);
                }

                break;
            }
        }
    }

    return true;
}



bool checkDuplicatePackages(QList<LocalRepo> & repos) {
    for (int i = 0; i < repos.size(); ++i) {
        LocalRepo *repo = &repos[i];
        QStringList duplicatePackages;

        // Find all duplicate Packages
        foreach (const QString package, repo->packages) {
            QString packageName = getNameofPKG(package);

            // Check for double packages
            QStringList foundPackages, packages = repo->packages.filter(packageName);

            for (int i = 0; i < packages.size(); ++i) {
                if (getNameofPKG(packages.at(i)) == packageName)
                    foundPackages.append(packages.at(i));
            }

            if (foundPackages.size() > 1)
                duplicatePackages.append(foundPackages);
        }

        // Remove duplicates and sort
        duplicatePackages.removeDuplicates();
        duplicatePackages.sort();

        if (duplicatePackages.isEmpty())
            continue;


        // Fill list of duplicate packages to remove
        QStringList removeDuplicatePackages = duplicatePackages;

        for (int i = 0; i < duplicatePackages.size(); ++i) {
            QString filename = duplicatePackages.at(i);
            QString name = getNameofPKG(filename);
            Version version = getVersionofPKG(filename);

            for (int x = 0; x < duplicatePackages.size(); ++x) {
                QString filenameCompare = duplicatePackages.at(x);
                Version versionCompare = getVersionofPKG(filenameCompare);

                if (x == i || name != getNameofPKG(filenameCompare) || version > versionCompare)
                    continue;

                filename = filenameCompare;
                version = versionCompare;
            }

            removeDuplicatePackages.removeAll(filename);
        }

        removeDuplicatePackages.removeDuplicates();
        removeDuplicatePackages.sort();


        // Inform the user if double packages were found
        while (true) {
            QString answer = getInput(QString(":: %1 packages with multiple versions were found in repository %2 %3!\n   Remove %4 old version(s)? [Y/n/d] (d=details) ").arg(QString::number(duplicatePackages.size()), repo->name, repo->architecture, QString::number(removeDuplicatePackages.size())), false, false).toLower().trimmed();
            if (answer == "d") {
                cout << endl << "duplicate packages:" << endl << endl;

                for (int i = 0; i < duplicatePackages.size(); ++i) {
                    cout << "   " << duplicatePackages.at(i).toUtf8().data() << endl;
                }

                cout << endl << "packages to remove:" << endl << endl;

                for (int i = 0; i < removeDuplicatePackages.size(); ++i) {
                    cout << "   " << removeDuplicatePackages.at(i).toUtf8().data() << endl;
                }

                cout << endl;
                continue;
            }
            else if (!answer.isEmpty() && answer != "y") {
                cerr << "abording..." << endl;
                return false;
            }
            else {
                // Remove old packages
                foreach (const QString file, removeDuplicatePackages) {
                    if (!QFile::remove(repo->path + "/" + file)) {
                        cerr << "error: failed to remove file '" << file.toUtf8().data() << "'!" << endl;
                        return false;
                    }

                    // Also remove package out of packages list
                    repo->packages.removeAll(file);
                }

                break;
            }
        }
    }

    return true;
}



bool checkSignatures(QList<LocalRepo> & repos) {
    const int signatureEndingLength = QString(BOXIT_SIGNATURE_ENDING).length();

    for (int i = 0; i < repos.size(); ++i) {
        LocalRepo *repo = &repos[i];
        QStringList corruptSignatures;

        // Find all signatures without package
        foreach (const QString signature, repo->signatures) {
            if (!repo->packages.contains(QString(signature).remove(signature.length() - signatureEndingLength, signatureEndingLength)))
                corruptSignatures.append(signature);
        }

        if (corruptSignatures.isEmpty())
            continue;

        while (true) {
            QString answer = getInput(QString(":: %1 signature file(s) without package in repository %2 %3!\n   Remove? [Y/n/d] (d=details) ").arg(QString::number(corruptSignatures.size()), repo->name, repo->architecture), false, false).toLower().trimmed();

            if (answer == "d") {
                cout << endl << "signatures without package:" << endl << endl;

                for (int i = 0; i < corruptSignatures.size(); ++i) {
                    cout << "   " << corruptSignatures.at(i).toUtf8().data() << endl;
                }

                cout << endl;
                continue;
            }
            else if (!answer.isEmpty() && answer != "y") {
                cerr << "abording..." << endl;
                return false;
            }
            else {
                // Remove corrupt signatures
                foreach (const QString file, corruptSignatures) {
                    if (!QFile::remove(repo->path + "/" + file)) {
                        cerr << "error: failed to remove file '" << file.toUtf8().data() << "'!" << endl;
                        return false;
                    }

                    // Also remove signature out of signatue list
                    repo->signatures.removeAll(file);
                }

                break;
            }
        }
    }

    return true;
}



bool checkMatchRepositories(QList<LocalRepo> & repos) {
    QMap<QString, QList<LocalRepo*> > repoMap;

    // Fill the repo map
    for (int i = 0; i < repos.size(); ++i) {
        LocalRepo *repo = &repos[i];

        repoMap[repo->name].append(repo);
    }

    // Check for different packages in the same repositories...
    QMapIterator<QString, QList<LocalRepo*> > it(repoMap);
    while (it.hasNext()) {
        it.next();

        QStringList differentPackages;

        for (int i = 0; i < it.value().size(); ++i) {
            const LocalRepo *repo = it.value().at(i);

            for (int x = 0; x < it.value().size(); ++x) {
                if (i == x)
                    continue;

                const LocalRepo *checkRepo = it.value().at(x);

                foreach(const QString package, repo->packages) {
                    const QString name = getNameofPKG(package);
                    const QString version = getVersionofPKG(package);
                    bool found = false;

                    foreach(const QString checkPackage, checkRepo->packages) {
                        if (name == getNameofPKG(checkPackage) && version == getVersionofPKG(checkPackage)) {
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                        differentPackages.append(package);
                }
            }
        }

        differentPackages.removeDuplicates();
        differentPackages.sort();

        if (differentPackages.isEmpty())
            continue;

        // Add repositories to missing packages...
        for (int i = 0; i < differentPackages.size(); ++i) {
            QString *package = &differentPackages[i];

            for (int x = 0; x < it.value().size(); ++x) {
                const LocalRepo *repo = it.value().at(x);

                if (repo->packages.contains(*package)) {
                    *package += "  (" + repo->name + " " + repo->architecture + ")";
                    break;
                }
            }
        }

        while (true) {
            QString answer = getInput(QString(":: Subrepositories of base repository '%1' contain %2 different package(s)!\n   Ignore and Continue? [Y/n/d] (d=details) ").arg(it.key(), QString::number(differentPackages.size())), false, false).toLower().trimmed();

            if (answer == "d") {
                cout << endl << "different packages:" << endl << endl;

                for (int i = 0; i < differentPackages.size(); ++i) {
                    cout << "   " << differentPackages.at(i).toUtf8().data() << endl;
                }

                cout << endl;
                continue;
            }
            else if (!answer.isEmpty() && answer != "y") {
                cerr << "abording..." << endl;
                return false;
            }
            else {
                break;
            }
        }
    }

    return true;
}



bool checkOverwriteSyncPackages(QList<LocalRepo> & repos) {
    for (int i = 0; i < repos.size(); ++i) {
        LocalRepo *repo = &repos[i];
        QStringList overwrittenPackages, newerSyncPackages;

        foreach (const QString package, repo->packages) {
            const QString name = getNameofPKG(package);
            const Version version = getVersionofPKG(package);

            // Check if same package exists in sync packages
            foreach (const QString syncPackage, repo->parentRepo->syncPackages) {
                if (name == getNameofPKG(syncPackage)) {
                    overwrittenPackages.append(syncPackage + "  ->  " + package);

                    // Check if sync package is newer than the local package
                    if (version < Version(getVersionofPKG(syncPackage)))
                        newerSyncPackages.append(syncPackage + "  ->  " + package);

                    break;
                }
            }
        }

        newerSyncPackages.removeDuplicates();
        overwrittenPackages.removeDuplicates();
        newerSyncPackages.sort();
        overwrittenPackages.sort();

        if (overwrittenPackages.isEmpty())
            continue;

        while (true) {
            QString answer = getInput(QString(":: %1 package(s) overwrite sync packages in repository %2 %3!\n   Continue? [Y/n/d] (d=details) ").arg(QString::number(overwrittenPackages.size()), repo->name, repo->architecture), false, false).toLower().trimmed();

            if (answer == "d") {
                cout << endl << "overwritten packages:" << endl;
                cout << endl << "   [OVERWRITTEN SYNC PACKAGE]  ->  [LOCAL PACKAGE]" << endl << endl;

                for (int i = 0; i < overwrittenPackages.size(); ++i) {
                    cout << "   " << overwrittenPackages.at(i).toUtf8().data() << endl;
                }

                cout << endl;
                continue;
            }
            else if (!answer.isEmpty() && answer != "y") {
                cerr << "abording..." << endl;
                return false;
            }
            else {
                break;
            }
        }

        if (newerSyncPackages.isEmpty())
            continue;

        while (true) {
            QString answer = getInput(QString(":: %1 overwritten sync package(s) are newer than local package(s) in repository %2 %3!\n   Continue? [Y/n/d] (d=details) ").arg(QString::number(newerSyncPackages.size()), repo->name, repo->architecture), false, false).toLower().trimmed();

            if (answer == "d") {
                cout << endl << "newer overwritten sync packages:" << endl;
                cout << endl << "   [OVERWRITTEN SYNC PACKAGE]  ->  [LOCAL PACKAGE]" << endl << endl;

                for (int i = 0; i < newerSyncPackages.size(); ++i) {
                    cout << "   " << newerSyncPackages.at(i).toUtf8().data() << endl;
                }

                cout << endl;
                continue;
            }
            else if (!answer.isEmpty() && answer != "y") {
                cerr << "abording..." << endl;
                return false;
            }
            else {
                break;
            }
        }
    }

    return true;
}



bool uploadData(const QString path, const int currentIndex, const int maxIndex) {
    if (!QFile::exists(path)) {
        cout << "\r                                                                               " << "\r" << flush;
        cerr << "error: file not found '" << path.toUtf8().data() << "'" << endl;
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        cout << "\r                                                                               " << "\r" << flush;
        cerr << "error: failed to open file '" << path.toUtf8().data() << "'" << endl;
        return false;
    }

    QString uploadFileName = path.split("/", QString::SkipEmptyParts).last();
    QString coutFileName = uploadFileName;

    if (coutFileName.size() > 40)
        coutFileName = coutFileName.remove(40, coutFileName.size()) + "...";


    // Tell the server the file checksum
    boxitSocket.sendData(MSG_FILE_CHECKSUM, sha1CheckSum(file.fileName()));
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cout << "\r                                                                               " << "\r" << flush;
        cerr << "\nerror: server failed to obtain file checksum!" << endl;
        return false;
    }


    boxitSocket.sendData(MSG_FILE_UPLOAD, QByteArray(uploadFileName.toUtf8()));
    boxitSocket.readData(msgID);

    if (msgID == MSG_FILE_ALREADY_EXISTS) {
        cout << "\r                                                                               " << "\r" << flush;
        cout << QString(":: [%1/%2] '%3' already exists [100%]").arg(QString::number(currentIndex), QString::number(maxIndex), coutFileName).toUtf8().data() << flush;
        return true;
    }
    else if (msgID == MSG_FILE_ALREADY_EXISTS_WRONG_CHECKSUM) {
        cout << "\r                                                                               " << "\r" << flush;
        cerr << "error: a different '" << uploadFileName.toUtf8().data() << "' file already exists on server! (Different checksums)" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cout << "\r                                                                               " << "\r" << flush;
        cerr << "error: server failed to obtain file '" << path.toUtf8().data() << "'" << endl;
        return false;
    }

    int dataSizeWritten = 0;
    int progress;

    while(!file.atEnd()) {
        data = file.read(BOXIT_SOCKET_MAX_SIZE);
        boxitSocket.sendData(MSG_DATA_UPLOAD, data);
        dataSizeWritten += data.size();

        // Calculate progress
        progress = file.size() / 100;
        if (progress <= 0)
            progress = 1;

        progress = dataSizeWritten / progress;

        // Output status
        cout << "\r                                                                               " << "\r" << flush;
        cout << QString(":: [%1/%2] uploading '%3' [%4%]").arg(QString::number(currentIndex), QString::number(maxIndex), coutFileName, QString::number(progress)).toUtf8().data() << flush;
    }
    file.close();

    // Tell the server that we finished...
    boxitSocket.sendData(MSG_DATA_UPLOAD_FINISH);
    boxitSocket.readData(msgID);

    if (msgID == MSG_ERROR_WRONG_CHECKSUM) {
        cout << "\r                                                                               " << "\r" << flush;
        cerr << "\nerror: uploaded file is corrupted '" << path.toUtf8().data() << "'" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cout << "\r                                                                               " << "\r" << flush;
        cerr << "\nerror: server failed to obtain file '" << path.toUtf8().data() << "'" << endl;
        return false;
    }

    cout << "\r                                                                               " << "\r" << flush;
    cout << QString(":: [%1/%2] finished '%3' [100%]").arg(QString::number(currentIndex), QString::number(maxIndex), coutFileName).toUtf8().data() << flush;

    return true;
}



bool listenOnStatus(bool exitOnSessionEnd) {
    TimeOutReset timeOutReset(&boxitSocket);
    QString host = "";

    if (!connectIfRequired(host))
        return false; // Error messages are printed by the method

    boxitSocket.sendData(MSG_LISTEN_ON_STATUS);
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to listen on status!" << endl;
        return false;
    }

    // Start our reset timeout timer
    timeOutReset.start();

    bool firstBranch = true;
    bool firstRepo = true;
    bool noState = true;
    bool returnValue = true;

    boxitSocket.readData(msgID, data);

    while (msgID == MSG_DATA_END_STATUS_LIST || msgID == MSG_DATA_NEW_STATUS_LIST || msgID == MSG_DATA_BRANCH_STATUS || msgID == MSG_DATA_REPO_STATUS || msgID == MSG_STATUS_SESSION_FAILED || msgID == MSG_STATUS_SESSION_FINISHED) {
        if (msgID == MSG_DATA_NEW_STATUS_LIST) {
            // Clear console
            write(1,"\E[H\E[2J",7);

            // Reset values
            firstBranch = firstRepo = noState = true;

            // Output info
            cout << ":: Status" << endl;
        }
        else if (msgID == MSG_DATA_BRANCH_STATUS) {
            noState = false;

            if (firstBranch) {
                cout << endl;
                consoleFill('-', 79);
                cout << setw(34) << "BRANCH" << setw(44) << "JOB" << endl;
                consoleFill('-', 79);
                firstBranch = false;
            }

            QStringList list = QString(data).split(BOXIT_SPLIT_CHAR, QString::KeepEmptyParts);
            if (list.size() < 4) {
                cerr << "error: invalid server reply!" << endl;
                return false;
            }

            cout << setw(34) << list.at(0).toUtf8().data() << setw(44) << list.at(1).toUtf8().data() << endl;
        }
        else if (msgID == MSG_DATA_REPO_STATUS) {
            noState = false;

            if (firstRepo) {
                cout << endl;
                consoleFill('-', 79);
                cout << setw(17) << "BRANCH" << setw(17) << "REPO" << setw(11) << "ARCH" << setw(33) << "JOB" << endl;
                consoleFill('-', 79);
                firstRepo = false;
            }

            QStringList list = QString(data).split(BOXIT_SPLIT_CHAR, QString::KeepEmptyParts);
            if (list.size() < 6) {
                cerr << "error: invalid server reply!" << endl;
                return false;
            }

            cout << setw(17) << list.at(0).toUtf8().data();
            cout << setw(17) << list.at(1).toUtf8().data();
            cout << setw(11) << list.at(2).toUtf8().data();
            cout << setw(33) << list.at(3).toUtf8().data() << endl;
        }
        else if (msgID == MSG_DATA_END_STATUS_LIST) {
            if (noState)
                cout << ":: No new repository or branch states." << endl;
        }
        else if (exitOnSessionEnd && msgID == MSG_STATUS_SESSION_FINISHED) {
            // Stop listining the status signals
            boxitSocket.sendData(MSG_STOP_LISTEN_ON_STATUS);

            // Set return value
            returnValue = true;
        }
        else if (exitOnSessionEnd && msgID == MSG_STATUS_SESSION_FAILED) {
            // Stop listining the status signals
            boxitSocket.sendData(MSG_STOP_LISTEN_ON_STATUS);

            // Set return value
            returnValue = false;
        }

        boxitSocket.readData(msgID, data);
    }

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to listen on status!" << endl;
        return false;
    }

    return returnValue;
}



bool printErrors() {
    QString host = "";
    if (!connectIfRequired(host))
        return false; // Error messages are printed by the method

    boxitSocket.sendData(MSG_LISTEN_ON_STATUS);
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to listen on status!" << endl;
        return false;
    }

    bool noErrors = true;

    boxitSocket.readData(msgID, data);

    while (msgID == MSG_DATA_NEW_STATUS_LIST || msgID == MSG_DATA_BRANCH_STATUS || msgID == MSG_DATA_REPO_STATUS || msgID == MSG_STATUS_SESSION_FAILED || msgID == MSG_STATUS_SESSION_FINISHED) {
        if (msgID == MSG_DATA_NEW_STATUS_LIST) {
            // Clear console
            write(1,"\E[H\E[2J",7);

            // Output info
            cout << ":: Obtaining branch and repository errors..." << endl;
        }
        else if (msgID == MSG_DATA_BRANCH_STATUS) {
            QStringList list = QString(data).split(BOXIT_SPLIT_CHAR, QString::KeepEmptyParts);
            if (list.size() < 4) {
                cerr << "error: invalid server reply!" << endl;
                return false;
            }

            // Print error if required
            if ((bool)list.at(2).toInt()) {
                noErrors = false;

                cout << endl;
                consoleFill('-', 79);
                cout << "Errors of branch " << list.at(0).toUtf8().data() << endl;
                consoleFill('-', 79);
                cout << list.at(3).toUtf8().data() << endl << endl;
            }
        }
        else if (msgID == MSG_DATA_REPO_STATUS) {
            QStringList list = QString(data).split(BOXIT_SPLIT_CHAR, QString::KeepEmptyParts);
            if (list.size() < 6) {
                cerr << "error: invalid server reply!" << endl;
                return false;
            }

            // Print error if required
            if ((bool)list.at(4).toInt()) {
                noErrors = false;

                cout << endl;
                consoleFill('-', 79);
                cout << "Errors of repository " << list.at(0).toUtf8().data() << " " << list.at(1).toUtf8().data() << " " << list.at(2).toUtf8().data() << endl;
                consoleFill('-', 79);
                cout << list.at(5).toUtf8().data() << endl << endl;
            }
        }

        boxitSocket.readData(msgID, data);
    }

    if (msgID != MSG_SUCCESS && msgID != MSG_DATA_END_STATUS_LIST) {
        cerr << "error: failed to listen on status!" << endl;
        return false;
    }

    if (noErrors)
        cout << ":: No new errors." << endl;

    return true;
}



bool syncBranch() {
    QString host = "";
    if (!connectIfRequired(host))
        return false; // Error messages are printed by the method

    // List all available branches
    QStringList branches;

    if (!getAllRemoteBranches(branches))
        return false; // Error messages are printed by the method

    cout << ":: Available branches:" << endl << endl;
    for (int i = 0; i < branches.size(); ++i) {
        cout << " " << QString::number(i + 1).toUtf8().data() << ") " << branches.at(i).toUtf8().data() << endl;
    }
    cout << endl;


    // Get user input
    int index;

    while (true) {
        index = getInput(":: Branch index: ", false, false).trimmed().toInt();

        if (index <= 0 || index > branches.size()) {
            cerr << "error: index is invalid!" << endl;
            continue;
        }

        break;
    }
    --index;

    const QString branchName = branches.at(index);


    // Ask user to change the current sync url
    if (!changeSyncUrlOnRequest(branchName))
        return false; // Error messages are printed by the method


    // Ask user to change exclude list
    QString answer = getInput(":: Adjust exclude list? [y/N] ", false, false).toLower().trimmed();
    if (answer == "y" && !changeSyncExcludeFiles(branchName))
        return false;


    // Send sync request
    cout << ":: Synchronizing branch '" << branchName.toUtf8().data() << "'" << endl;

    boxitSocket.sendData(MSG_SYNC_BRANCH, QByteArray(branchName.toUtf8()));
    boxitSocket.readData(msgID);

    if (msgID == MSG_IS_LOCKED) {
        cerr << "error: at least one branch repository is already locked by another process!" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to start synchronization process! The branch might not exists or another synchronization process is running..." << endl;
        return false;
    }

    if (!listenOnStatus(true)) {
        cout << endl << ":: Synchronization failed! Check the process errors..." << endl;
        return false; // Error messages are printed by the method
    }

    cout << endl << ":: Synchronization successfully finished." << endl;

    return true;
}



bool changeSyncUrlOnRequest(const QString branchName) {
    // Get current sync url of branch
    boxitSocket.sendData(MSG_GET_BRANCH_URL, QByteArray(branchName.toUtf8()));
    boxitSocket.readData(msgID, data);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to obtain branch sync url!" << endl;
        return false;
    }

    // Ask user to change url
    QString answer = getInput(QString(":: Current synchronization url: '%1'\n   Change it? [y/N] ").arg(QString(data)), false, false).toLower().trimmed();
    if (answer != "y")
        return true;

    // Get new url
    QString input;

    cout << " hint: possible variables: $branch, $repo and $arch" << endl;

    while (true) {
        input = getInput(" new synchronization url: ", false, false).trimmed();

        // Check if valid
        if (input.isEmpty() || (!input.contains("http://") && !input.contains("ftp://"))) {
            cerr << "error: url is invalid. It must contain the 'http://' or 'ftp://' prefix." << endl;
            continue;
        }

        break;
    }

    // Set new url
    boxitSocket.sendData(MSG_SET_BRANCH_URL, QByteArray(QString(branchName + BOXIT_SPLIT_CHAR + input).toUtf8()));
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to set new branch sync url!" << endl;
        return false;
    }

    return true;
}



bool changeSyncExcludeFiles(const QString branchName) {
    boxitSocket.sendData(MSG_GET_BRANCH_SYNC_EXCLUDE_FILES, QByteArray(QString(branchName).toUtf8()));
    boxitSocket.readData(msgID, data);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to get exclude content!" << endl;
        return false;
    }

    // Write content to temporary file to edit it
    QFile file(QString(BOXIT_TMP_PATH) + "/.boxit-" + QString::number(qrand()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cerr << "error: failed to create file '" << file.fileName().toUtf8().data() << "'!" << endl;
        return false;
    }

    QTextStream out(&file);
    out << QString(data);
    file.close();

    // Show hint
    cout << " hint: wildcards (?, *, [...]) and comments (#) are allowed." << endl;

    // Get editor to use
    QString editor = getInput(" editor: ", false, false).trimmed();


    // Set our interrupt signal
    signal(SIGINT, interruptEditorProcess);

    // Show hint
    cout << " hint: kill editor with CTRL+C." << endl;

    editorProcess->start(editor, QStringList() << file.fileName());
    editorProcess->waitForStarted();

    while(editorProcess->state() == QProcess::Running) {
        // Reset our server timeout to keep our session alive!
        resetServerTimeout();

        // wait...
        editorProcess->waitForFinished(5000);
    }

    // Reset our interrupt signal
    signal(SIGINT, SIG_DFL);

    // Warn user on possible process failure
    if (editorProcess->exitCode() != 0)
        cerr << "warning: process might have failed!" << endl;

    // Get file content
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        cerr << "error: failed to read file content!" << endl;
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Remove file again
    file.remove();

    // Just wait, so we don't get false keyboard inputs
    usleep(100000);

    // Ask user to upload new exclude list
    QString answer = getInput(" upload new exclude list? [Y/n] ", false, false).toLower().trimmed();
    if (!answer.isEmpty() && answer != "y") {
        cerr << "abording..." << endl;
        return false;
    }

    // Upload new exclude list
    boxitSocket.sendData(MSG_SET_BRANCH_SYNC_EXCLUDE_FILES, QByteArray(QString(branchName + BOXIT_SPLIT_CHAR + content).toUtf8()));
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to set new exclude content!" << endl;
        return false;
    }

    return true;
}



bool changePassword() {
    QString host = "";
    if (!connectIfRequired(host))
        return false; // Error messages are printed by the method

    cout << ":: Set new password" << endl;

    QString oldPasswd = getInput(" old password: ", true, false);
    QString newPasswd = getInput(" new password: ", true, false);
    QString confirmNewPasswd = getInput(" confirm password: ", true, false);

    if (newPasswd != confirmNewPasswd) {
        cerr << "error: entered passwords are not equal!" << endl;
        return false;
    }
    else if (newPasswd.size() < 6) {
        cerr << "error: new password is too short! Use at least 6 words..." << endl;
        return false;
    }
    else if (newPasswd == oldPasswd) {
        cerr << "error: new password is equal to the current password!" << endl;
        return false;
    }

    // Create the password hash
    oldPasswd = QString(QCryptographicHash::hash(oldPasswd.toLocal8Bit(), QCryptographicHash::Sha1).toHex());
    newPasswd = QString(QCryptographicHash::hash(newPasswd.toLocal8Bit(), QCryptographicHash::Sha1).toHex());

    boxitSocket.sendData(MSG_SET_PASSWD, QByteArray(QString(oldPasswd + BOXIT_SPLIT_CHAR + newPasswd).toUtf8()));
    boxitSocket.readData(msgID);

    if (msgID == MSG_ERROR_WRONG_PASSWORD) {
        cerr << "error: entered password was wrong!" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to set new password!" << endl;
        return false;
    }

    cout << ":: Password changed." << endl;

    return true;
}



bool snapshotBranch() {
    QString host = "";
    if (!connectIfRequired(host))
        return false; // Error messages are printed by the method

    // List all available branches
    QStringList branches;

    if (!getAllRemoteBranches(branches))
        return false; // Error messages are printed by the method

    cout << ":: Available branches:" << endl << endl;
    for (int i = 0; i < branches.size(); ++i) {
        cout << " " << QString::number(i + 1).toUtf8().data() << ") " << branches.at(i).toUtf8().data() << endl;
    }
    cout << endl;


    // Get user input
    int index;

    while (true) {
        index = getInput(":: Source Branch index: ", false, false).trimmed().toInt();

        if (index <= 0 || index > branches.size()) {
            cerr << "error: index is invalid!" << endl;
            continue;
        }

        break;
    }
    --index;

    const QString sourceBranchName = branches.at(index);


    while (true) {
        index = getInput(":: Destination Branch index: ", false, false).trimmed().toInt();

        if (index <= 0 || index > branches.size()) {
            cerr << "error: index is invalid!" << endl;
            continue;
        }

        break;
    }
    --index;

    const QString destBranchName = branches.at(index);


    if (sourceBranchName == destBranchName) {
        cerr << "error: source and destination branch can't be the same!" << endl;
        return false;
    }

    // Ask user to continue
    QString answer = getInput(QString(":: Create a snapshot of branch '%1' and save it to branch '%2'.\n   Continue? [y/N] ").arg(sourceBranchName, destBranchName), false, false).toLower().trimmed();
    if (answer != "y") {
        cerr << "abording..." << endl;
        return false;
    }


    // Send snap request
    boxitSocket.sendData(MSG_SNAP_BRANCH, QByteArray(QString(sourceBranchName + BOXIT_SPLIT_CHAR + destBranchName).toUtf8()));
    boxitSocket.readData(msgID);

    if (msgID == MSG_IS_LOCKED) {
        cerr << "error: at least one branch repository is already locked by another process!" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to create a snapshot of branch '" << sourceBranchName.toUtf8().data() << "'!" << endl;
        return false;
    }

    cout << ":: Snapshot created." << endl;

    return true;
}
