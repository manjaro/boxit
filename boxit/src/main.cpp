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
#include <readline/readline.h>
#include <readline/history.h>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QList>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include "interactiveprocess.h"
#include "boxitsocket.h"
#include "const.h"
#include "version.h"
#include "timeoutreset.h"

using namespace std;



enum ARGUMENTS {
    ARG_NONE = 0x0000,
    ARG_PULL = 0x0001,
    ARG_PUSH = 0x0002,
    ARG_INIT = 0x0004,
    ARG_SHELL = 0x0008
};


struct Config {
    QString url, state, repository, architecture;
};



// General
void printHelp();
void drawLogo();
void catchSignal(int);
void interruptEditorProcess(int sig);
void interruptProcessOutput(int sig);
bool socketOperation();
bool setConfig();
bool readConfig();

// Package operations
QString getNameofPKG(QString pkg);
Version getVersionofPKG(QString pkg);
QString getArchofPKG(QString pkg);
bool checkValidArchitecture();
bool checkDuplicatePackages();
bool checkForSignatures();
QByteArray sha1CheckSum(QString filePath);

// Socket operations
bool connectAndLoginToHost(QString host);
bool lockRepo(QString repository, QString architecture);
bool unlockRepo();
bool printServerMessages(QString repository, QString architecture, bool allowStop);
void printMessageHistory(QString history);
bool getRepoTMPSyncState(bool &hasTMPSync, QString repository, QString architecture);
bool getRepositoryState(QString &state, QString repository, QString architecture);
bool getRemoteFiles(QStringList &remoteFiles, QString repository, QString architecture);
bool getRemoteSignatures(QStringList &remoteSignatures, QString repository, QString architecture);
bool getRemotePackages(QStringList &remotePackages, QString repository, QString architecture);
bool performInit();
bool performPull();
bool performPush();
bool uploadData(QString path);
void resetServerTimeout();

// BoxIt shell
bool runShell();
inline bool basicInputCheck(QString &repository, QString &architecture);


// Input
void consoleEchoMode(bool on = true);
void consoleFill(char fill, int wide);
QString getInput(QString beg = "", bool hide = false, bool addToHistory = true, bool resetTimeout = true);

// Readline
static char** completion(const char*, int ,int);
char* generator(const char*,int);
char * dupstr (char*);
void * xmalloc (int);

// Auto completion
const char* cmd[] = { "www.manjaro.org", "manjaro.org", "clear", "quit", "exit", "help", "passwd", "list", "rebuilddb", "sync", "addrepo", "rmrepo", "copyrepo", "moverepo", "syncurl", "exedit", "kill", "pshow" };


// Variables
ARGUMENTS arguments;
BoxitSocket boxitSocket;
Config config;
quint16 msgID;
QByteArray data;
QString username;
InteractiveProcess *editorProcess = NULL;




int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Set readline completion
    rl_attempted_completion_function = completion;

    //enable auto-complete
    rl_bind_key('\t', rl_complete);


    // Setup signals
    signal(SIGHUP, catchSignal);
    signal(SIGINT, catchSignal);
    signal(SIGTERM, catchSignal);
    signal(SIGKILL, catchSignal);

    arguments = ARG_NONE;

    // Get command line arguments
    for (int nArg=1; nArg < argc; nArg++) {
        if (strcmp(argv[nArg], "help") == 0) {
            printHelp();
            return 0;
        }
        else if (strcmp(argv[nArg], "shell") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_SHELL);
        }
        else if (strcmp(argv[nArg], "pull") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_PULL);
        }
        else if (strcmp(argv[nArg], "push") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_PUSH);
        }
        else if (strcmp(argv[nArg], "init") == 0) {
            arguments = (ARGUMENTS)(arguments | ARG_INIT);

            if (++nArg >= argc) {
                cerr << "option 'init' requires a boxit url as argument!\nSample: host@repository:architecture" << endl;
                return 1;
            }

            QString work = argv[nArg];
            config.url = work.split("@", QString::KeepEmptyParts).first();

            if (!work.contains("@") || !work.contains(":") || config.url.isEmpty()) {
                cerr << "error: the passed url is invalid!\nSample: host@repository:architecture" << endl;
                return 1;
            }

            work = work.split("@", QString::SkipEmptyParts).last();
            config.architecture = work.split(":", QString::KeepEmptyParts).last();
            config.repository = work.split(":", QString::KeepEmptyParts).first();

            if (config.repository.isEmpty() || config.architecture.isEmpty()) {
                cerr << "error: the passed url is invalid!\nSample: host@repository:architecture" << endl;
                return 1;
            }
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
    else if (arguments & ARG_SHELL && (arguments & ARG_PULL || arguments & ARG_PUSH || arguments & ARG_INIT)) {
        cerr << "error: cannot run BoxIt shell with push, pull or init arguments!" << endl;
        return 1;
    }



    // Check if .boxit/config exists and read it
    if (QFile::exists(BOXIT_CONFIG)) {
        if (!readConfig())
            return 1; // Error messages are printed by the function...
    }
    else if (arguments & ARG_PULL || arguments & ARG_PUSH) {
        cerr << "error: cannot push or pull without a config! Init first..." << endl;
        return 1;
    }


    //###
    //### Perform operations
    //###

    // Run BoxIt shell
    if (arguments & ARG_SHELL && !runShell())
        return 1;

    // Check for duplicate packages and missing signatures
    if (arguments & ARG_PUSH && (!checkValidArchitecture() || !checkDuplicatePackages() || !checkForSignatures()))
        return 1;

    // Perform socket job
    if ((arguments & ARG_INIT || arguments & ARG_PULL || arguments & ARG_PUSH) && !socketOperation())
        return 1;


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

    if (editorProcess != NULL)
        editorProcess->kill();
}



void interruptProcessOutput(int sig) {
    signal(sig, SIG_DFL);
    boxitSocket.sendData(MSG_STOP_REPO_MESSAGES);
}



void printHelp() {
    drawLogo();
    cout << "Roland Singer <roland@manjaro.org>" << endl << endl;

    cout << "Usage: boxit [OPTIONS] [...]" << endl << endl;
    cout << "  help\t\t\tshow help" << endl;
    cout << "  init host@repo:arch\tinitiate a first repo and pull from it" << endl;
    cout << "  pull\t\t\tupdate an existing repo" << endl;
    cout << "  push\t\t\tupload all changes" << endl;
    cout << "  shell\t\t\trun BoxIt shell" << endl;
    cout << endl;
}



void drawLogo() {
    cout << " ____  _____  _  _  ____  ____ " << endl;
    cout << "(  _ \\(  _  )( \\/ )(_  _)(_  _)" << endl;
    cout << " ) _ < )(_)(  )  (  _)(_   )(  " << endl;
    cout << "(____/(_____)(_/\\_)(____) (__) " << endl << endl;
}



bool socketOperation() {
    if (config.url.isEmpty() || config.repository.isEmpty() || config.architecture.isEmpty()) {
        cerr << "error: url, architecture or repository is somehow empty!?" << endl;
        return false;
    }

    // Connect to host
    if (!connectAndLoginToHost(config.url))
        return false; // Error messages are printed by the function


    // Perform operations
    if (arguments & ARG_INIT && (!performInit() || !performPull()))
        goto error; // Error messages are printed by the functions
    else if (arguments & ARG_PUSH && !performPush())
        goto error; // Error messages are printed by the function
    else if (arguments & ARG_PULL && !performPull())
        goto error; // Error messages are printed by the function


    // Disconnect from server
    boxitSocket.disconnectFromHost();
    return true;

error:
    // Disconnect from server
    boxitSocket.disconnectFromHost();
    return false;
}



bool readConfig() {
    QFile file(BOXIT_CONFIG);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cerr << "error: failed to read file '" << BOXIT_CONFIG << "'!" << endl;
        return false;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().split("#", QString::KeepEmptyParts).first().trimmed();
        if (line.isEmpty() || !line.contains("="))
            continue;

        QString arg1 = line.split("=").first().toLower().trimmed(), arg2 = line.split("=").last().trimmed();

        if (arg1 == "remoteurl")
            config.url = arg2;
        else if (arg1 == "remoterepository")
            config.repository = arg2;
        else if (arg1 == "remotearchitecture")
            config.architecture = arg2;
        else if (arg1 == "remotestate")
            config.state = arg2;
    }
    file.close();

    if (config.url.isEmpty() || config.repository.isEmpty() || config.architecture.isEmpty()) {
        cerr << "error: config '" << BOXIT_CONFIG << "' is incomplete!" << endl;
        return false;
    }

    return true;
}



bool setConfig() {
    // Create config
    QFile file(BOXIT_CONFIG);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cerr << "error: failed to create BoxIt config '" << BOXIT_CONFIG << "'!" << endl;
        return false;
    }

    QTextStream out(&file);
    out << "###\n### BoxIt Config\n###\n";
    out << "\nremoteurl = " << config.url;
    out << "\nremoterepository = " << config.repository;
    out << "\nremotearchitecture = " << config.architecture;
    out << "\nremotestate = " << config.state;

    file.close();

    return true;
}



//###
//### Package operations
//###



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



bool checkValidArchitecture() {
    QStringList invalidFiles, dummyFiles = QDir(QDir::currentPath()).entryList(QString(BOXIT_FILE_FILTERS).split(" ", QString::SkipEmptyParts), QDir::NoDotAndDotDot | QDir::Files | QDir::System, QDir::Name);
    QStringList skipArchs = QString(BOXIT_SKIP_ARCHITECTURE_CHECKS).split(" ", QString::SkipEmptyParts);

    for (int i = 0; i < dummyFiles.size(); ++i) {
        QString currentFile = dummyFiles.at(i);
        QString arch = getArchofPKG(currentFile);

        if (!skipArchs.contains(arch) && arch != config.architecture)
            invalidFiles.append(currentFile);
    }

    // Inform the user if invalid packages were found
    if (!invalidFiles.isEmpty()) {
        while (true) {
            QString answer = getInput(QString::number(invalidFiles.size()) + " package(s) with invalid architecture found!\nRemove them? [Y/n/d] (d=details) ", false, false).toLower().trimmed();
            if (answer == "d") {
                cout << endl << "packages to remove:" << endl << endl;

                for (int i = 0; i < invalidFiles.size(); ++i) {
                    cout << "   " << invalidFiles.at(i).toAscii().data() << endl;
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
                for (int i = 0; i < invalidFiles.size(); ++i) {
                    if (!QFile::remove(invalidFiles.at(i))) {
                        cerr << "error: failed to remove file '" << invalidFiles.at(i).toAscii().data() << "'!" << endl;
                        return false;
                    }
                }

                break;
            }
        }
    }

    return true;
}



bool checkDuplicatePackages() {
    QStringList duplicatePackages, dummyFiles = QDir(QDir::currentPath()).entryList(QString(BOXIT_FILE_FILTERS).split(" ", QString::SkipEmptyParts), QDir::NoDotAndDotDot | QDir::Files | QDir::System, QDir::Name);

    // Find all duplicate Packages
    for (int i = 0; i < dummyFiles.size(); ++i) {
        QString currentFile = dummyFiles.at(i);
        QString packageName = getNameofPKG(currentFile);

        // Check for double packages
        QStringList foundPackages, packages = dummyFiles.filter(packageName);
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

    // Inform the user if double packages were found
    if (!duplicatePackages.isEmpty()) {
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


        while (true) {
            QString answer = getInput(QString::number(duplicatePackages.size()) + " packages with multiple versions were found!\nRemove " + QString::number(removeDuplicatePackages.size()) + " old version(s)? [Y/n/d] (d=details) ", false, false).toLower().trimmed();
            if (answer == "d") {
                cout << endl << "duplicate packages:" << endl << endl;

                for (int i = 0; i < duplicatePackages.size(); ++i) {
                    cout << "   " << duplicatePackages.at(i).toAscii().data() << endl;
                }

                cout << endl << "packages to remove:" << endl << endl;

                for (int i = 0; i < removeDuplicatePackages.size(); ++i) {
                    cout << "   " << removeDuplicatePackages.at(i).toAscii().data() << endl;
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
                for (int i = 0; i < removeDuplicatePackages.size(); ++i) {
                    if (!QFile::remove(removeDuplicatePackages.at(i))) {
                        cerr << "error: failed to remove file '" << removeDuplicatePackages.at(i).toAscii().data() << "'!" << endl;
                        return false;
                    }
                }

                break;
            }
        }
    }

    return true;
}



bool checkForSignatures() {
    QStringList corruptSignatures, dummyFiles = QDir(QDir::currentPath()).entryList(QStringList() << "*" + QString(BOXIT_SIGNATURE_ENDING), QDir::NoDotAndDotDot | QDir::Files | QDir::System, QDir::Name);


    // Find all signatures without package
    for (int i = 0; i < dummyFiles.size(); ++i) {
        QString currentFile = dummyFiles.at(i);

        if (!QFile::exists(QString(currentFile).remove(currentFile.length() - QString(BOXIT_SIGNATURE_ENDING).length(), QString(BOXIT_SIGNATURE_ENDING).length())))
            corruptSignatures.append(currentFile);
    }

    if (!corruptSignatures.isEmpty()) {
        while (true) {
            QString answer = getInput(QString::number(corruptSignatures.size()) + " signature file(s) without package! Remove? [Y/n/d] (d=details) ", false, false).toLower().trimmed();

            if (answer == "d") {
                cout << endl << "signatures without package:" << endl << endl;

                for (int i = 0; i < corruptSignatures.size(); ++i) {
                    cout << "   " << corruptSignatures.at(i).toAscii().data() << endl;
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
                for (int i = 0; i < corruptSignatures.size(); ++i) {
                    if (!QFile::remove(corruptSignatures.at(i))) {
                        cerr << "error: failed to remove file '" << corruptSignatures.at(i).toAscii().data() << "'!" << endl;
                        return false;
                    }
                }

                break;
            }
        }
    }



    // Find all packages with missing signatures
    corruptSignatures.clear();
    dummyFiles = QDir(QDir::currentPath()).entryList(QString(BOXIT_FILE_FILTERS).split(" ", QString::SkipEmptyParts), QDir::NoDotAndDotDot | QDir::Files | QDir::System, QDir::Name);

    for (int i = 0; i < dummyFiles.size(); ++i) {
        QString currentFile = dummyFiles.at(i);

        if (!QFile::exists(currentFile + BOXIT_SIGNATURE_ENDING))
            corruptSignatures.append(currentFile);
    }

    if (corruptSignatures.isEmpty())
        return true;


    while (true) {
        QString answer;
        if (corruptSignatures.size() == 1)
            answer = getInput(QString::number(corruptSignatures.size()) + " package without signature was found! Ignore? [N/y/d] (d=details) ", false, false).toLower().trimmed();
        else
            answer = getInput(QString::number(corruptSignatures.size()) + " packages without signature were found! Ignore? [N/y/d] (d=details) ", false, false).toLower().trimmed();


        if (answer == "d") {
            cout << endl << "packages without signatures:" << endl << endl;

            for (int i = 0; i < corruptSignatures.size(); ++i) {
                cout << "   " << corruptSignatures.at(i).toAscii().data() << endl;
            }

            cout << endl;
            continue;
        }
        else if (answer.isEmpty() || answer != "y") {
            cerr << "abording..." << endl;
            return false;
        }
        else {
            cout << "ignoring warning..." << endl;
            break;
        }
    }

    return true;
}



//###
//### Socket operations
//###



bool connectAndLoginToHost(QString host) {
    // Connect to host
    if (!boxitSocket.connectToHost(host))
        return false; // Error messages are printed by the boxit socket class


    // First send our fuchs version to the server
    boxitSocket.sendData(MSG_CHECK_VERSION, QByteArray(QString::number((int)BOXIT_VERSION).toAscii()));
    boxitSocket.readData(msgID);
    if (msgID != MSG_SUCCESS) {
        cerr << "error: the BoxIt server is running a different version! Abording..." << endl;
        goto error;
    }


    // Let's login now
    for (int i=0;; i++) {
        username = getInput("username: ", false, false, false);
        QString password = QString(QCryptographicHash::hash(getInput("password: ", true, false, false).toLocal8Bit(), QCryptographicHash::Sha1).toHex());

        boxitSocket.sendData(MSG_AUTHENTICATE, QByteArray(QString(username + BOXIT_SPLIT_CHAR + password).toAscii()));
        boxitSocket.readData(msgID);

        if (msgID == MSG_SUCCESS) {
            break;
        }
        else {
            cerr << "Failed to login!" << endl;
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



bool lockRepo(QString repository, QString architecture) {
    // Lock repository
    boxitSocket.sendData(MSG_LOCK, QByteArray(QString(repository + BOXIT_SPLIT_CHAR + architecture).toAscii()));
    boxitSocket.readData(msgID);
    if (msgID == MSG_ERROR_REPO_LOCK_ONLY_ONCE) {
        cerr << "error: one repository is already locked!" << endl;
        return false;
    }
    else if (msgID == MSG_ERROR_REPO_ALREADY_LOCKED) {
        cerr << "error: repository is in use!" << endl;
        return false;
    }
    else if (msgID == MSG_ERROR_NOT_EXIST) {
        cerr << "error: repository does not exist!" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to lock repository!" << endl;
        return false;
    }

    return true;
}



bool unlockRepo() {
    // Unlock repository
    boxitSocket.sendData(MSG_UNLOCK);
    boxitSocket.readData(msgID);
    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to unlock repository!" << endl;
        return false;
    }

    return true;
}



bool printServerMessages(QString repository, QString architecture, bool allowStop) {
    boxitSocket.sendData(MSG_GET_REPO_MESSAGES, QByteArray(QString(repository + BOXIT_SPLIT_CHAR + architecture).toAscii()));
    boxitSocket.readData(msgID, data);

    if (msgID == MSG_ERROR_NOT_EXIST) {
        cerr << "error: repository does not exist!" << endl;
        return false;
    }
    else if (msgID == MSG_ERROR_NOT_BUSY) {
        // Print message history
        printMessageHistory(QString(data));
        cout<< "> repository has no running process!" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to request repository process messages!" << endl;
        return false;
    }


    // Set our interrupt signal if allowed
    if (allowStop) {
        cout << "hint: stop process output with CTRL+C" << endl;
        signal(SIGINT, interruptProcessOutput);
    }

    // Our timeout reset
    TimeOutReset timeOutReset(&boxitSocket);
    timeOutReset.start();

    // Print message history
    printMessageHistory(QString(data));


    // Get messages...
    bool lastFlush = false;
    boxitSocket.readData(msgID, data);

    while (msgID == MSG_MESSAGE) {
        QString msg = QString(data);

        // Check if we should flush
        if (msg.startsWith(BOXIT_FLUSH_STRING)) {
            msg.remove(0, QString(BOXIT_FLUSH_STRING).size());
            cout << "\r" << flush;
            cout << msg.toAscii().data() << flush;
            lastFlush = true;
        }
        else {
            if (lastFlush) {
                lastFlush = false;
                cout << endl;
            }
            cout << msg.toAscii().data() << endl;
        }

        boxitSocket.readData(msgID, data);
    }

    // Reset our interrupt signal
    signal(SIGINT, SIG_DFL);

    timeOutReset.stop();

    if (msgID == MSG_PROCESS_BACKGROUND) {
        cout << "> sending process to background..." << endl;
        return true;
    }
    else if (msgID == MSG_PROCESS_FINISHED) {
        cout << "> finished process!" << endl;
        return true;
    }
    else if (msgID == MSG_PROCESS_FAILED) {
        cout << "> process failed!" << endl;
        return false;
    }
    else {
        cout << "> server answered with an unkown command!" << endl;
        return false;
    }
}



void printMessageHistory(QString history) {
    QStringList list = history.split("\n", QString::KeepEmptyParts);
    QString last = list.last();
    list.removeLast();

    // Check if we should flush
    if (last.startsWith(BOXIT_FLUSH_STRING)) {
        last.remove(0, QString(BOXIT_FLUSH_STRING).size());

        cout << list.join("\n").toAscii().data() << endl;
        cout << "\r" << flush;
        cout << last.toAscii().data() << flush;
    }
    else {
        cout << history.toAscii().data() << endl;
    }
}



bool getRepoTMPSyncState(bool &hasTMPSync, QString repository, QString architecture) {
    // Check if a temporary sync is existing for this repo
    boxitSocket.sendData(MSG_REPO_HAS_TMP_SYNC, QByteArray(QString(repository + BOXIT_SPLIT_CHAR + architecture).toAscii()));
    boxitSocket.readData(msgID, data);

    if (msgID == MSG_ERROR_NOT_EXIST) {
        cerr << "error: repository does not exist!" << endl;
        return false;
    }
    else if (msgID == MSG_ERROR_IS_BUSY) {
        cerr << "error: repository is busy!" << endl;
        return false;
    }
    else if (msgID != MSG_YES && msgID != MSG_NO) {
        cerr << "error: failed to determind repository synchronization state!" << endl;
        return false;
    }

    hasTMPSync = (msgID == MSG_YES);

    return true;
}



bool getRepositoryState(QString &state, QString repository, QString architecture) {
    boxitSocket.sendData(MSG_REPOSITORY_STATE, QByteArray(QString(repository + BOXIT_SPLIT_CHAR + architecture).toAscii()));
    boxitSocket.readData(msgID, data);

    if (msgID == MSG_ERROR_NOT_EXIST) {
        cerr << "error: repository does not exist!" << endl;
        return false;
    }
    else if (msgID == MSG_ERROR_IS_BUSY) {
        cerr << "error: repository is busy!" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to obtain repository state!" << endl;
        return false;
    }

    state = QString(data);
    return true;
}



bool getRemoteFiles(QStringList &remoteFiles, QString repository, QString architecture) {
    QStringList remoteSignatures;

    if (!getRemotePackages(remoteFiles, repository, architecture) || !getRemoteSignatures(remoteSignatures, repository, architecture))
        return false;

    remoteFiles.append(remoteSignatures);
    return true;
}



bool getRemoteSignatures(QStringList &remoteSignatures, QString repository, QString architecture) {
    remoteSignatures.clear();

    boxitSocket.sendData(MSG_SIGNATURE_LIST, QByteArray(QString(repository + BOXIT_SPLIT_CHAR + architecture).toAscii()));
    boxitSocket.readData(msgID, data);

    while (msgID == MSG_SIGNATURE) {
        remoteSignatures.append(QString(data));
        boxitSocket.readData(msgID, data);
    }

    if (msgID == MSG_ERROR_NOT_EXIST) {
        cerr << "error: repository does not exist!" << endl;
        return false;
    }
    else if (msgID == MSG_ERROR_IS_BUSY) {
        cerr << "error: repository is busy!" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to obtain signature list!" << endl;
        return false;
    }

    return true;
}



bool getRemotePackages(QStringList &remotePackages, QString repository, QString architecture) {
    remotePackages.clear();

    boxitSocket.sendData(MSG_PACKAGE_LIST, QByteArray(QString(repository + BOXIT_SPLIT_CHAR + architecture).toAscii()));
    boxitSocket.readData(msgID, data);

    while (msgID == MSG_PACKAGE) {
        remotePackages.append(QString(data));
        boxitSocket.readData(msgID, data);
    }

    if (msgID == MSG_ERROR_NOT_EXIST) {
        cerr << "error: repository does not exist!" << endl;
        return false;
    }
    else if (msgID == MSG_ERROR_IS_BUSY) {
        cerr << "error: repository is busy!" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to obtain package list!" << endl;
        return false;
    }

    return true;
}



bool performInit() {
    // Check if already a boxit config exists
    if (QFile(BOXIT_CONFIG).exists()) {
        cerr << "error: a BoxIt config already exists! Maybe you want to use pull?" << endl;
        return false;
    }

    // Check if directory is empty
    if (!QDir().entryList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::System, QDir::Name).isEmpty()) {
        cerr << "error: the current directory is not empty!" << endl;
        return false;
    }


    // Create directory
    if (!QDir().exists(BOXIT_DIR))
        QDir().mkdir(BOXIT_DIR);


    // Create dummy file
    QFile file(BOXIT_DUMMY_FILE);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cerr << "error: failed to create BoxIt dummy file '" << BOXIT_DUMMY_FILE << "'!" << endl;
        return false;
    }

    QTextStream out(&file);
    out << "###\n### BoxIt Dummy File\n###\n";
    file.close();

    // Create config
    if (!setConfig())
        return false;

    return true;
}



bool performPull() {
    QString repositoryState;
    QStringList removeDummies, packages;
    QStringList dummyFiles = QDir(QDir::currentPath()).entryList(QString(BOXIT_FILE_FILTERS_WITH_SIG).split(" ", QString::SkipEmptyParts), QDir::NoDotAndDotDot | QDir::Files | QDir::System, QDir::Name);

    // Get repository state
    if (!getRepositoryState(repositoryState, config.repository, config.architecture))
        return false; // Error messages are handled by the function

    // Get remote packages
    if (!getRemoteFiles(packages, config.repository, config.architecture))
        return false; // Error messages are handled by the function


    for (int i = 0; i < dummyFiles.size(); ++i) {
        QString currentDummy = dummyFiles.at(i);
        QFileInfo info(currentDummy);

        if (!packages.contains(currentDummy) || !info.isSymLink() || !info.symLinkTarget().endsWith(BOXIT_DUMMY_FILE))
            removeDummies.append(currentDummy);
        else
            packages.removeAll(currentDummy);
    }

    if (config.state == repositoryState && removeDummies.size() == 0 && packages.size() == 0) {
        cout << "repository is up-to-date!" << endl;
        return true;
    }
    else if (config.state == repositoryState && (removeDummies.size() != 0 || packages.size() != 0)) {
        cout << "warning: repository is up-to-date but files don't match with the remote ones!" << endl;
    }



    // Inform user
    while (true) {
        QString answer = getInput("Remove " + QString::number(removeDummies.size()) + " local file(s) and create " + QString::number(packages.size()) + " dummy file(s)? [Y/n/d] (d=details) ", false, false).toLower().trimmed();
        if (answer == "d") {
            if (!packages.isEmpty()) {
                cout << endl << "dummy file(s) to create:" << endl << endl;

                for (int i = 0; i < packages.size(); ++i) {
                    cout << "   " << packages.at(i).toAscii().data() << endl;
                }
            }

            if (!removeDummies.isEmpty()) {
                cout << endl << "local file(s) to remove:" << endl << endl;

                for (int i = 0; i < removeDummies.size(); ++i) {
                    cout << "   " << removeDummies.at(i).toAscii().data() << endl;
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


    // Remove files
    for (int i = 0; i < removeDummies.size(); ++i) {
        if (!QFile::remove(removeDummies.at(i))) {
            cerr << "error: failed to remove '" << removeDummies.at(i).toAscii().data() << "'!" << endl;
            return false;
        }
    }

    // Create dummies
    QFile dummyFile(BOXIT_DUMMY_FILE);
    for (int i = 0; i < packages.size(); ++i) {
        if (!dummyFile.link(packages.at(i))) {
            cerr << "error: failed to create symlink '" << packages.at(i).toAscii().data() << "'!" << endl;
            return false;
        }
    }


    // Set repository state and update config
    config.state = repositoryState;
    if (!setConfig()) {
        cerr << "error: failed to update config!" << endl;
        return false;
    }


    cout << "repository is up-to-date now!" << endl;
    return true;
}



bool performPush() {
    // Get repository state
    QString repositoryState;
    if (!getRepositoryState(repositoryState, config.repository, config.architecture))
        return false; // Error messages are handled by the function

    if (repositoryState != config.state) {
        cerr << "error: repository is not up-to-date! Perform a pull action first!" << endl;
        return false;
    }


    QStringList addPackages, removePackages, remotePackages;
    QStringList dummyFiles = QDir(QDir::currentPath()).entryList(QString(BOXIT_FILE_FILTERS_WITH_SIG).split(" ", QString::SkipEmptyParts), QDir::NoDotAndDotDot | QDir::Files | QDir::System, QDir::Name);


    // Get remote packages
    if (!getRemoteFiles(remotePackages, config.repository, config.architecture))
        return false; // Error messages are handled by the function


    // Find all added packages
    for (int i = 0; i < dummyFiles.size(); ++i) {
        QString currentFile = dummyFiles.at(i);
        QFileInfo fileInfo(currentFile);

        if (remotePackages.contains(currentFile) && fileInfo.isSymLink())
                continue;

        if (!fileInfo.isFile() || fileInfo.isSymLink()) {
            cerr << "error: file '" << currentFile.toAscii().data() << "' is not a valid file!" << endl;
            return false;
        }

        addPackages.append(currentFile);
    }


    // Find all removed packages
    for (int i = 0; i < remotePackages.size(); ++i) {
        QString currentFile = remotePackages.at(i);

        if (!dummyFiles.contains(currentFile))
            removePackages.append(currentFile);
    }


    if (removePackages.isEmpty() && addPackages.isEmpty()) {
        cerr << "nothing to do!" << endl;
        return false;
    }



    // Inform user
    while (true) {
        QString answer = getInput("Remove " + QString::number(removePackages.size()) + " remote file(s) and upload " + QString::number(addPackages.size()) + " package(s) ? [Y/n/d] (d=details) ", false, false).toLower().trimmed();
        if (answer == "d") {
            if (!removePackages.isEmpty()) {
                cout << endl << "remote packages to remove:" << endl << endl;

                for (int i = 0; i < removePackages.size(); ++i) {
                    cout << "   " << removePackages.at(i).toAscii().data() << endl;
                }
            }

            if (!addPackages.isEmpty()) {
                cout << endl << "packages to upload:" << endl << endl;

                for (int i = 0; i < addPackages.size(); ++i) {
                    cout << "   " << addPackages.at(i).toAscii().data() << endl;
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




    // Check if a temporary sync is existing for this repo
    bool hasTMPSync;
    if (!getRepoTMPSyncState(hasTMPSync, config.repository, config.architecture))
       return false; // Error messages are printed by the function

    if (hasTMPSync) {
        QString answer = getInput("Repository has a temporary synchronization! You should commit this first! Do you really want to discard this files to continue? [y/N] ", false, false).toLower().trimmed();
        if (answer.isEmpty() || answer != "y") {
            cerr << "abording..." << endl;
            return false;
        }
    }


    // Lock repository
    if (!lockRepo(config.repository, config.architecture))
        return false; // Error messages are printed by the function



    // Send all packages to remove
    for (int i = 0; i < removePackages.size(); ++i) {
        boxitSocket.sendData(MSG_REMOVE_PACKAGE, QByteArray(removePackages.at(i).toAscii()));
        boxitSocket.readData(msgID);

        if (msgID != MSG_SUCCESS) {
            cerr << "error: failed to send remove request to server!" << endl;
            return false;
        }
    }


    // Upload new packages
    for (int i = 0; i < addPackages.size(); ++i) {
        if (!uploadData(addPackages.at(i)))
            return false; // Error handling is done by the function
    }


    // Commit changes
    boxitSocket.sendData(MSG_COMMIT);
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: failed to start commit process!" << endl;
        return false;
    }

    // Unlock repository
    unlockRepo(); // Error messages are printed by the function -> not fatal


    // Read process messages
    if (!printServerMessages(config.repository, config.architecture, false))
        return false; // Error handling is done by the function


    // Remove files and replace them with dummy files
    QFile dummyFile(BOXIT_DUMMY_FILE);
    for (int i = 0; i < addPackages.size(); ++i) {
        QString file = addPackages.at(i);

        if (QFile::exists(file) && !QFile::remove(file)) {
            cerr << "error: failed to remove '" << file.toAscii().data() << "'!" << endl;
            return false;
        }

        if (!dummyFile.link(file)) {
            cerr << "error: failed to create symlink '" << file.toAscii().data() << "'!" << endl;
            return false;
        }
    }


    // Update repository state
    if (!getRepositoryState(repositoryState, config.repository, config.architecture))
        return false; // Error messages are handled by the functio

    // Set repository state and update config
    config.state = repositoryState;
    if (!setConfig()) {
        cerr << "error: failed to update config!" << endl;
        return false;
    }


    cout << "successfully committed changes!" << endl;
    return true;
}



bool uploadData(QString path) {
    if (!QFile::exists(path)) {
        cerr << "error: file not found '" << path.toAscii().data() << "'" << endl;
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        cerr << "error: failed to open file '" << path.toAscii().data() << "'" << endl;
        return false;
    }

    QString uploadFileName = path.split("/", QString::SkipEmptyParts).last();

    boxitSocket.sendData(MSG_FILE_UPLOAD, QByteArray(uploadFileName.toAscii()));
    boxitSocket.readData(msgID);

    if (msgID != MSG_SUCCESS) {
        cerr << "error: server failed to obtain file '" << path.toAscii().data() << "'" << endl;
        return false;
    }

    int dataSizeWritten = 0;

    while(!file.atEnd()) {
        data = file.read(50000);
        boxitSocket.sendData(MSG_DATA_UPLOAD, data);
        dataSizeWritten += data.size();

        cout << "\r" << flush;
        cout << "uploading '" << uploadFileName.toAscii().data() << "' (" << dataSizeWritten/(file.size()/100) << "%)" << flush;
    }
    file.close();

    // Tell the server that we finished...
    boxitSocket.sendData(MSG_DATA_UPLOAD_FINISH, sha1CheckSum(file.fileName()));
    boxitSocket.readData(msgID);

    if (msgID == MSG_ERROR_CHECKSUM_WRONG) {
        cerr << "\nerror: uploaded file is corrupted '" << path.toAscii().data() << "'" << endl;
        return false;
    }
    else if (msgID != MSG_SUCCESS) {
        cerr << "\nerror: server failed to obtain file '" << path.toAscii().data() << "'" << endl;
        return false;
    }

    cout << "\r" << flush;
    cout << "upload '" << uploadFileName.toAscii().data() << "' complete (100%)" << endl;
    return true;
}



void resetServerTimeout() {
    if (boxitSocket.state() == QAbstractSocket::ConnectedState)
        boxitSocket.sendData(MSG_RESET_TIMEOUT);
}







//###
//### Shell Section
//###




bool runShell() {
    // Init
    editorProcess = new InteractiveProcess(qApp);

    // Connect to host
    if (!connectAndLoginToHost(getInput("host: ", false, false).trimmed()))
        return false; // Error messages are printed by the function

    //  Draw Logo
    write(1,"\E[H\E[2J",7);
    drawLogo();


    QStringList input;

    while (boxitSocket.state() == QAbstractSocket::ConnectedState) {
        data.clear();

        // Get user input
        input = getInput("[" + username + "@BoxIt]$ ").trimmed().split(" ", QString::SkipEmptyParts);



        // Read user input
        if (input.isEmpty()) {
            continue;
        }
        else if (input.at(0) == "clear") {
            write(1,"\E[H\E[2J",7);
        }
        else if (input.at(0) == "quit" || input.at(0) == "exit") {
            // Cleanup
            delete editorProcess;

            // Disconnect
            boxitSocket.disconnectFromHost();

            cout << "Good bye!" << endl;
            qApp->quit();
            return true;
        }
        else if (input.at(0) == "help") {
            drawLogo();
            cout << "Roland Singer <roland@manjaro.org>" << endl << endl;

            cout << "Usage: [COMMAND] [OPTIONS] [...]" << endl << endl;
            cout << "  clear\t\t\tclear terminal" << endl;
            cout << "  quit/exit\t\texit shell" << endl;
            cout << "  help\t\t\tshow help" << endl;
            cout << "  passwd\t\tchange password" << endl;
            cout << "  list\t\t\tshow available repositories" << endl;
            cout << "  rebuilddb\t\trebuild repository database" << endl;
            cout << "  sync [package] [...]\tsynchronise repository" << endl;
            cout << "  addrepo\t\tadd repository" << endl;
            cout << "  rmrepo\t\tremove repository" << endl;
            cout << "  copyrepo\t\tcopy repository" << endl;
            cout << "  moverepo\t\tmove repository" << endl;
            cout << "  syncurl\t\tshow and change repository synchronization url" << endl;
            cout << "  exedit\t\tchange repository exclude file" << endl;
            cout << "  kill\t\t\tkill repository process" << endl;
            cout << "  pshow\t\t\tshow repository process output" << endl;
            cout << endl;
        }
        else if (input.at(0) == "passwd") {
            QString oldPasswd = getInput("old password: ", true, false);
            QString newPasswd = getInput("new password: ", true, false);
            QString confirmNewPasswd = getInput("confirm password: ", true, false);

            if (newPasswd != confirmNewPasswd) {
                cerr << "error: entered passwords are not equal!" << endl;
                continue;
            }
            else if (newPasswd.size() < 6) {
                cerr << "error: new password is too short! Use at least 6 words..." << endl;
                continue;
            }
            else if (newPasswd == oldPasswd) {
                cerr << "error: new password is equal to the current password!" << endl;
                continue;
            }

            // Create the password hash
            oldPasswd = QString(QCryptographicHash::hash(oldPasswd.toLocal8Bit(), QCryptographicHash::Sha1).toHex());
            newPasswd = QString(QCryptographicHash::hash(newPasswd.toLocal8Bit(), QCryptographicHash::Sha1).toHex());

            boxitSocket.sendData(MSG_SET_PASSWD, QByteArray(QString(oldPasswd + BOXIT_SPLIT_CHAR + newPasswd).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_WRONG_PASSWORD) {
                cerr << "error: entered password was wrong!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to set new password!" << endl;
                continue;
            }

            cout << "successfully changed password!" << endl;
        }
        else if (input.at(0) == "list") {
            boxitSocket.sendData(MSG_REQUEST_LIST);
            boxitSocket.readData(msgID, data);

            QStringList split;
            bool firstRepo = true;

            while (msgID == MSG_LIST_REPO) {
                if (firstRepo) {
                    cout << endl;
                    consoleFill('-', 79);
                    cout << setw(30) << "REPO" << setw(10) << "ARCH" << setw(19) << "JOB" << setw(19) << "STATE" << endl;
                    consoleFill('-', 79);
                    firstRepo = false;
                }

                split = QString(data).split(BOXIT_SPLIT_CHAR, QString::SkipEmptyParts);

                if (split.size() < 4) {
                    cout << QString(data).toAscii().data() << endl;
                }
                else {
                    cout << setw(30) << split.at(0).toAscii().data();
                    cout << setw(10) << split.at(1).toAscii().data();
                    cout << setw(19) << split.at(2).toAscii().data();
                    cout << setw(19) << split.at(3).toAscii().data() << endl;
                }


                boxitSocket.readData(msgID, data);
            }

            cout << endl;

            if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to get repository list!" << endl;
                continue;
            }
        }
        else if (input.at(0) == "rebuilddb") {
            QString name = getInput("repository name: ", false, false).trimmed();
            QString architecture = getInput("repository architecture: ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function


            // Send data
            boxitSocket.sendData(MSG_REPO_DB_REBUILD, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to start rebuild database process!" << endl;
                continue;
            }

            cout << "Started repository database rebuild process..." << endl;
        }
        else if (input.at(0) == "sync") {
            QString name = getInput("repository name: ", false, false).trimmed();
            QString architecture = getInput("repository architecture: ", false, false).trimmed();

            // Custom sync packages
            input.removeFirst();
            QString customSyncPackages = input.join(" ").trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function



            // Inform user to always update the sync exclude list
            cout << "Repository synchronization exclude list should always be up-to-date!" << endl;

            for (int i=3; i>0; i--) {
                cout << "\r" << flush;
                cout << "Wait " << i << " second(s)..." << flush;
                sleep(1);
            }

            cout << "\r" << flush;

            QString answer = getInput("Continue synchronization? [Y/n] ", false, false).toLower().trimmed();
            if (!answer.isEmpty() && answer != "y") {
                cerr << "abording..." << endl;
                continue;
            }



            // Check if a temporary sync is existing for this repo
            bool hasTMPSync, discard = true;
            if (!getRepoTMPSyncState(hasTMPSync, name, architecture))
               continue; // Error messages are printed by the function

            if (hasTMPSync) {
                answer = getInput("Repository has a temporary synchronization! Do you want to discard this or sync on top of it? Discard? [y/N] ", false, false).toLower().trimmed();
                if (answer.isEmpty() || answer != "y")
                    discard = false;
            }


            // Send data
            boxitSocket.sendData(MSG_REPO_SYNC, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture + BOXIT_SPLIT_CHAR  + QString::number(discard) + BOXIT_SPLIT_CHAR + customSyncPackages).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_NO_SYNC_REPO) {
                cerr << "error: repository does not have a synchronization url!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to start synchronization process!" << endl;
                continue;
            }

            cout << "Started repository synchronization process..." << endl;

            // Read process messages
            if (!printServerMessages(name, architecture, true))
                continue; // Error handling is done by the function
        }
        else if (input.at(0) == "kill") {
            QString name = getInput("repository name: ", false, false).trimmed();
            QString architecture = getInput("repository architecture: ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function

            QString answer = getInput("Do you really want to kill the repository process? [y/N] ", false, false).toLower().trimmed();
            if (answer.isEmpty() || answer != "y") {
                cerr << "abording..." << endl;
                continue;
            }

            // Send data
            boxitSocket.sendData(MSG_KILL, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_NOT_BUSY) {
                cerr << "error: repository process is not running!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to kill repository process! Is it locked by an user?" << endl;
                continue;
            }

            cout << "Successfully killed repository process!" << endl;
        }
        else if (input.at(0) == "pshow") {
            QString name = getInput("repository name: ", false, false).trimmed();
            QString architecture = getInput("repository architecture: ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function

            if (!printServerMessages(name, architecture, true))
                continue; // Error messages are printed by the function
        }
        else if (input.at(0) == "addrepo") {
            QString name = getInput("repository name: ", false, false).trimmed();
            QString architecture = getInput("repository architecture: ", false, false).trimmed();
            QString syncURL = getInput("synchronization url (leave blank to disable): ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function
            else if (!syncURL.isEmpty()
                     && (!syncURL.contains("@")
                         || (!syncURL.split("@", QString::SkipEmptyParts).last().startsWith("http://")
                             && !syncURL.split("@", QString::SkipEmptyParts).last().startsWith("ftp://")))) {
                cerr << "error: synchronization url is invalid!\nsample: reponame@http://host.org/repo or reponame@ftp://host.org/repo" << endl;
                continue;
            }

            // Question
            QString answer = getInput(QString("Create repository '%1 %2'? [y/N] ").arg(name, architecture), false, false).toLower().trimmed();
            if (answer.isEmpty() || answer != "y") {
                cerr << "abording..." << endl;
                continue;
            }


            // Send data
            boxitSocket.sendData(MSG_REPO_ADD, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_ALREADY_EXIST) {
                cerr << "error: repository already exists!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to create new repository!" << endl;
                continue;
            }


            // Set sync url if not empty
            if (!syncURL.isEmpty()) {
                boxitSocket.sendData(MSG_REPO_CHANGE_SYNC_URL, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture + BOXIT_SPLIT_CHAR + syncURL).toAscii()));
                boxitSocket.readData(msgID);

                if (msgID == MSG_ERROR_NOT_EXIST) {
                    cerr << "error: repository does not exist!" << endl;
                    continue;
                }
                else if (msgID == MSG_ERROR_IS_BUSY) {
                    cerr << "error: repository is busy!" << endl;
                    continue;
                }
                else if (msgID != MSG_SUCCESS) {
                    cerr << "error: failed to set new repository synchronization url!" << endl;
                    continue;
                }
            }


            cout << "Successfully added new repository!" << endl;
        }
        else if (input.at(0) == "rmrepo") {
            QString name = getInput("repository name: ", false, false).trimmed();
            QString architecture = getInput("repository architecture: ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function

            // Question
            QString answer = getInput(QString("Do you really want to delete repository '%1 %2'? [y/N] ").arg(name, architecture), false, false).toLower().trimmed();
            if (answer.isEmpty() || answer != "y") {
                cerr << "abording..." << endl;
                continue;
            }


            // Send data
            boxitSocket.sendData(MSG_REPO_REMOVE, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to remove repository!" << endl;
                continue;
            }

            cout << "Repository deletion started!" << endl;
        }
        else if (input.at(0) == "copyrepo") {
            QString name = getInput("source repository name: ", false, false).trimmed();
            QString architecture = getInput("source repository architecture: ", false, false).trimmed();
            QString destName = getInput("destination repository name: ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function

            // Question
            QString answer = getInput(QString("Do you really want to copy repository '%1 %2' to '%3 %2'? [y/N] ").arg(name, architecture, destName), false, false).toLower().trimmed();
            if (answer.isEmpty() || answer != "y") {
                cerr << "abording..." << endl;
                continue;
            }


            // Send data
            boxitSocket.sendData(MSG_REPO_COPY, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture + BOXIT_SPLIT_CHAR + destName).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_ALREADY_EXIST) {
                cerr << "error: destination repository already exists!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to copy repository!" << endl;
                continue;
            }

            cout << "Repository copy started!" << endl;
        }
        else if (input.at(0) == "moverepo") {
            QString name = getInput("source repository name: ", false, false).trimmed();
            QString architecture = getInput("source repository architecture: ", false, false).trimmed();
            QString destName = getInput("destination repository name: ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function

            // Question
            QString answer = getInput(QString("Do you really want to move repository '%1 %2' to '%3 %2'? [y/N] ").arg(name, architecture, destName), false, false).toLower().trimmed();
            if (answer.isEmpty() || answer != "y") {
                cerr << "abording..." << endl;
                continue;
            }


            // Send data
            boxitSocket.sendData(MSG_REPO_MOVE, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture + BOXIT_SPLIT_CHAR + destName).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_ALREADY_EXIST) {
                cerr << "error: destination repository already exists!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to move repository!" << endl;
                continue;
            }

            cout << "Repository move started!" << endl;
        }
        else if (input.at(0) == "syncurl") {
            QString name = getInput("repository name: ", false, false).trimmed();
            QString architecture = getInput("repository architecture: ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function


            // Get sync url
            boxitSocket.sendData(MSG_REPO_GET_SYNC_URL, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture).toAscii()));
            boxitSocket.readData(msgID, data);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to get repository synchronization url!" << endl;
                continue;
            }

            if (QString(data).isEmpty())
                cout << "no synchronization url" << endl;
            else
                cout << "synchronization url: " << QString(data).toAscii().data() << endl;


            // Question
            QString answer = getInput(QString("Change synchronization url [y/N] "), false, false).toLower().trimmed();
            if (answer.isEmpty() || answer != "y")
                continue;


            // Set new sync url
            QString syncURL = getInput("new synchronization url (leave blank to disable): ", false, false).trimmed();

            if (syncURL.isEmpty()) {
                syncURL = "!";
            }
            else if (!syncURL.contains("@")
                     || (!syncURL.split("@", QString::SkipEmptyParts).last().startsWith("http://")
                         && !syncURL.split("@", QString::SkipEmptyParts).last().startsWith("ftp://"))) {
                cerr << "error: synchronization url is invalid!\nsample: reponame@http://host.org/repo or reponame@ftp://host.org/repo" << endl;
                continue;
            }


            boxitSocket.sendData(MSG_REPO_CHANGE_SYNC_URL, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture + BOXIT_SPLIT_CHAR + syncURL).toAscii()));
            boxitSocket.readData(msgID);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to set new repository synchronization url!" << endl;
                continue;
            }

            cout << "Successfully changed repository synchronization url!" << endl;
        }
        else if (input.at(0) == "exedit") {
            QString name = getInput("repository name: ", false, false).trimmed();
            QString architecture = getInput("repository architecture: ", false, false).trimmed();

            // Perform basic checks
            if (!basicInputCheck(name, architecture))
                continue; // Error messages are printed by the function

            boxitSocket.sendData(MSG_GET_EXCLUDE_CONTENT, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture).toAscii()));
            boxitSocket.readData(msgID, data);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to get exclude content!" << endl;
                continue;
            }

            // Write content to temporary file to edit it
            QFile file("/tmp/.boxit-" + QString::number(qrand()));
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                cerr << "error: failed to create file '" << file.fileName().toAscii().data() << "'!" << endl;
                continue;
            }

            QTextStream out(&file);
            out << QString(data);
            file.close();


            // Get editor to use
            QString editor = getInput("editor: ", false, false).trimmed();


            // Set our interrupt signal
            signal(SIGINT, interruptEditorProcess);

            cout << "kill editor with CTRL+C" << endl;

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

            // Warn user on possible process fail
            if (editorProcess->exitCode() != 0)
                cerr << "warning: process might failed!" << endl;

            // Get file content
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
                cerr << "error: failed to read file content!" << endl;
                continue;
            }

            QTextStream in(&file);
            QString content = in.readAll();
            file.close();

            // Remove file again
            file.remove();

            // Just wait, so we don't get fails keyboard inputs
            usleep(100000);

            // Ask user to upload new content
            QString answer = getInput("Upload new content? [Y/n] ", false, false).toLower().trimmed();
            if (!answer.isEmpty() && answer != "y") {
                cerr << "abording..." << endl;
                continue;
            }

            // Upload new content
            boxitSocket.sendData(MSG_SET_EXCLUDE_CONTENT, QByteArray(QString(name + BOXIT_SPLIT_CHAR + architecture + BOXIT_SPLIT_CHAR + content).toAscii()));
            boxitSocket.readData(msgID, data);

            if (msgID == MSG_ERROR_NOT_EXIST) {
                cerr << "error: repository does not exist!" << endl;
                continue;
            }
            else if (msgID == MSG_ERROR_IS_BUSY) {
                cerr << "error: repository is busy!" << endl;
                continue;
            }
            else if (msgID != MSG_SUCCESS) {
                cerr << "error: failed to set new exclude content!" << endl;
                continue;
            }

            cout << "Successfully changed new exclude content!" << endl;
        }
        else {
            cout << "unkown command" << endl;
        }
    }

    // Cleanup
    delete editorProcess;

    return true;
}



inline bool basicInputCheck(QString &repository, QString &architecture) {
    // Perform basic checks
    if (repository.isEmpty() || architecture.isEmpty()) {
        cerr << "error: name and architecture can't be empty!" << endl;
        return false;
    }
    else if (!QString(BOXIT_ARCHITECTURES).split(" ", QString::SkipEmptyParts).contains(architecture)) {
        cerr << "error: invalid architecture!\navailable architectures: " << BOXIT_ARCHITECTURES << endl;
        return false;
    }

    return true;
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



QString getInput(QString beg, bool hide, bool addToHistory, bool resetTimeout) {
    // Reset server timeout
    if (resetTimeout)
        resetServerTimeout();

    QString str;

    if (hide)
        consoleEchoMode(false);

    str = QString(readline(beg.toAscii().data()));

    if (hide) {
        consoleEchoMode(true);
        cout << endl;
    }

    if (str.contains(BOXIT_SPLIT_CHAR)) {
        cerr << "Your input contains an invalid character: '" << BOXIT_SPLIT_CHAR << "'" << endl;
        return getInput(beg, hide);
    }

    if (!str.isEmpty() && addToHistory)
        add_history(str.trimmed().toAscii().data());

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
