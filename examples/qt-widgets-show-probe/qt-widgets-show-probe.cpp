#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <cstdio>

int main(int argc, char **argv) {
    int passed = 0;
    int failed = 0;
    QApplication app(argc, argv);

    QMainWindow simple;
    auto *label = new QLabel(QStringLiteral("BionicX Qt widgets"), &simple);
    simple.setCentralWidget(label);
    simple.resize(480, 280);
    simple.setWindowTitle(QStringLiteral("BionicX Qt widgets"));
    simple.show();
    if (simple.isVisible()) {
        std::fprintf(stdout, "BXTEST PASS qt-widgets-show visible\n");
        ++passed;
    } else {
        std::fprintf(stdout, "BXTEST FAIL qt-widgets-show hidden\n");
        ++failed;
    }
    std::fflush(stdout);

    QMainWindow chrome;
    chrome.setCentralWidget(new QTreeView(&chrome));
    chrome.menuBar()->addMenu(QStringLiteral("File"))->addAction(QStringLiteral("Quit"));
    chrome.addToolBar(QStringLiteral("Main"))->addAction(QStringLiteral("Open"));
    chrome.statusBar()->showMessage(QStringLiteral("ready"));
    QSystemTrayIcon tray;
    tray.setToolTip(QStringLiteral("BionicX"));
    tray.setVisible(true);
    chrome.resize(640, 400);
    chrome.setWindowTitle(QStringLiteral("BionicX Qt chrome"));
    chrome.show();
    if (chrome.isVisible() && chrome.menuBar()->isVisible()) {
        std::fprintf(stdout, "BXTEST PASS qt-widgets-chrome visible\n");
        ++passed;
    } else {
        std::fprintf(stdout, "BXTEST FAIL qt-widgets-chrome hidden\n");
        ++failed;
    }
    std::fflush(stdout);

    QTimer::singleShot(800, &app, &QCoreApplication::quit);
    const int rc = app.exec();
    if (rc == 0) {
        std::fprintf(stdout, "BXTEST PASS qt-widgets-exec rc=0\n");
        ++passed;
    } else {
        std::fprintf(stdout, "BXTEST FAIL qt-widgets-exec rc=%d\n", rc);
        ++failed;
    }
    std::fprintf(stdout, "BXSUMMARY qt-widgets-show passed=%d failed=%d\n",
                 passed, failed);
    std::fflush(stdout);
    return failed ? 1 : 0;
}
