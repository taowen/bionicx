#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <cstdio>

// Mirrors KeePassXC 2.7.10 DatabaseWidget: a QStackedWidget that creates
// extra QWidget children with parent=this, then reparents them into nested
// QSplitters, plus an EditEntry-like tab page. The real app builds this
// tree while the MainWindow is still hidden, then bringToFront() shows it.
static QStackedWidget *buildDatabaseWidgetLike(QWidget *parent) {
    auto *stack = new QStackedWidget(parent);
    auto *mainWidget = new QWidget(stack);
    auto *mainSplitter = new QSplitter(mainWidget);
    auto *groupSplitter = new QSplitter(stack);
    auto *previewSplitter = new QSplitter(mainWidget);
    auto *searchingLabel = new QLabel(QStringLiteral("Searching…"), stack);
    auto *groupView = new QTreeView(stack);
    auto *tagView = new QTreeView(stack);
    auto *preview = new QWidget(stack);
    auto *editPage = new QWidget(stack);
    auto *openPage = new QWidget(stack);

    auto *model = new QStandardItemModel(stack);
    model->appendRow(new QStandardItem(QStringLiteral("login")));
    groupView->setModel(model);
    tagView->setModel(model);

    auto *tagsWidget = new QWidget();
    auto *tagsLayout = new QVBoxLayout(tagsWidget);
    tagsLayout->setContentsMargins(0, 0, 0, 0);
    tagsLayout->addWidget(new QLabel(QStringLiteral("Searches and Tags")));
    tagsLayout->addWidget(tagView);

    groupSplitter->setOrientation(Qt::Vertical);
    groupSplitter->setChildrenCollapsible(true);
    groupSplitter->addWidget(groupView);
    groupSplitter->addWidget(tagsWidget);
    groupSplitter->setStretchFactor(0, 100);
    groupSplitter->setStretchFactor(1, 0);
    groupSplitter->setSizes({1, 1});

    auto *rhs = new QWidget(mainSplitter);
    auto *rhsLayout = new QVBoxLayout(rhs);
    rhsLayout->setContentsMargins(0, 0, 0, 0);
    searchingLabel->setVisible(false);
    rhsLayout->addWidget(searchingLabel);
    rhsLayout->addWidget(previewSplitter);

    auto *entryView = new QTreeView(rhs);
    entryView->setModel(model);
    preview->hide();
    previewSplitter->setOrientation(Qt::Vertical);
    previewSplitter->addWidget(entryView);
    previewSplitter->addWidget(preview);
    previewSplitter->setStretchFactor(0, 100);
    previewSplitter->setStretchFactor(1, 0);
    previewSplitter->setSizes({1, 1});

    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->addWidget(groupSplitter);
    mainSplitter->addWidget(rhs);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 100);
    mainSplitter->setSizes({1, 1});

    auto *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->addWidget(mainSplitter);

    auto *editLayout = new QVBoxLayout(editPage);
    auto *tabs = new QTabWidget(editPage);
    editLayout->addWidget(tabs);
    for (int i = 0; i < 6; ++i) {
        auto *page = new QWidget;
        auto *form = new QVBoxLayout(page);
        for (int j = 0; j < 8; ++j)
            form->addWidget(new QLineEdit(page));
        tabs->addTab(page, QStringLiteral("Tab %1").arg(i));
    }

    auto *openLayout = new QVBoxLayout(openPage);
    openLayout->addWidget(new QLabel(QStringLiteral("Unlock database")));
    openLayout->addWidget(new QLineEdit(openPage));

    stack->addWidget(mainWidget);
    stack->addWidget(editPage);
    stack->addWidget(openPage);
    stack->setCurrentWidget(mainWidget);
    return stack;
}

static void check(int *passed, int *failed, const char *name, bool ok) {
    if (ok) {
        std::fprintf(stdout, "BXTEST PASS %s\n", name);
        ++*passed;
    } else {
        std::fprintf(stdout, "BXTEST FAIL %s\n", name);
        ++*failed;
    }
    std::fflush(stdout);
}

int main(int argc, char **argv) {
    int passed = 0;
    int failed = 0;
    QApplication app(argc, argv);

    QMainWindow welcome;
    welcome.setCentralWidget(new QLabel(QStringLiteral("Welcome"), &welcome));
    welcome.menuBar()->addMenu(QStringLiteral("Database"));
    welcome.addToolBar(QStringLiteral("Main"))->addAction(QStringLiteral("Open"));
    welcome.statusBar()->showMessage(QStringLiteral("ready"));
    welcome.resize(720, 480);
    welcome.setWindowTitle(QStringLiteral("BionicX welcome"));
    welcome.show();
    check(&passed, &failed, "welcome-show", welcome.isVisible());

    QMainWindow dbWin;
    auto *tabs = new QTabWidget(&dbWin);
    dbWin.setCentralWidget(tabs);
    dbWin.menuBar()->addMenu(QStringLiteral("Database"))->addAction(QStringLiteral("Open"));
    dbWin.addToolBar(QStringLiteral("Main"))->addAction(QStringLiteral("Lock"));
    dbWin.statusBar()->showMessage(QStringLiteral("db"));
    dbWin.resize(960, 600);
    dbWin.setWindowTitle(QStringLiteral("BionicX DatabaseWidget"));
    auto *page = buildDatabaseWidgetLike(tabs);
    tabs->addTab(page, QStringLiteral("bionicx.kdbx"));
    dbWin.show();
    check(&passed, &failed, "db-tree-show",
          dbWin.isVisible() && page->isVisible() && page->currentWidget() != nullptr);

    QApplication::processEvents();
    check(&passed, &failed, "db-tree-current",
          page->currentWidget() != nullptr && page->currentWidget()->isVisible());

    QTimer::singleShot(800, &app, &QCoreApplication::quit);
    const int rc = app.exec();
    check(&passed, &failed, "db-tree-exec", rc == 0);

    std::fprintf(stdout, "BXSUMMARY keepassxc-db-widget passed=%d failed=%d\n",
                 passed, failed);
    std::fflush(stdout);
    return failed ? 1 : 0;
}
