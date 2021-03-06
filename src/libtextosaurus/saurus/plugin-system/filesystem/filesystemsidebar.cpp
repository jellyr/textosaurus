// This file is distributed under GNU GPLv3 license. For full license text, see <project-git-repository-root-folder>/LICENSE.md.

#include "saurus/plugin-system/filesystem/filesystemsidebar.h"

#include "common/gui/basetextedit.h"
#include "common/gui/plaintoolbutton.h"
#include "common/miscellaneous/iconfactory.h"
#include "definitions/definitions.h"
#include "saurus/miscellaneous/application.h"
#include "saurus/miscellaneous/syntaxhighlighting.h"
#include "saurus/miscellaneous/textapplication.h"
#include "saurus/plugin-system/filesystem/favoriteslistwidget.h"
#include "saurus/plugin-system/filesystem/filesystemmodel.h"
#include "saurus/plugin-system/filesystem/filesystemplugin.h"
#include "saurus/plugin-system/filesystem/filesystemview.h"

#include <QComboBox>
#include <QFileSystemModel>
#include <QGroupBox>
#include <QListWidget>
#include <QMimeData>
#include <QStorageInfo>
#include <QToolBar>
#include <QVBoxLayout>

FilesystemSidebar::FilesystemSidebar(FilesystemPlugin* plugin, QWidget* parent)
  : BaseSidebar(plugin->textApp(), parent), m_plugin(plugin), m_fsModel(nullptr) {
  setWindowTitle(tr("Filesystem"));
  setObjectName(QSL("m_sidebarFilesystem"));

  connect(this, &FilesystemSidebar::openFileRequested, this, [this](const QString& file_path) {
    m_plugin->textApp()->loadTextEditorFromFile(file_path);
  });
}

Qt::DockWidgetArea FilesystemSidebar::initialArea() const {
  return Qt::DockWidgetArea::LeftDockWidgetArea;
}

bool FilesystemSidebar::initiallyVisible() const {
  return true;
}

int FilesystemSidebar::initialWidth() const {
  return 250;
}

void FilesystemSidebar::reloadDrives() {
  auto storages = QStorageInfo::mountedVolumes();

  std::sort(storages.begin(), storages.end(), [](const QStorageInfo& lhs, const QStorageInfo& rhs) {
    return QString::compare(lhs.rootPath(), rhs.rootPath(), Qt::CaseSensitivity::CaseInsensitive) < 0;
  });

  for (const QStorageInfo& strg : storages) {
    QString name_drive = QDir::toNativeSeparators(strg.rootPath());

    if (!strg.name().isEmpty()) {
      name_drive += QString(" (%1)").arg(strg.name());
    }

    if (!strg.fileSystemType().isEmpty()) {
      name_drive += QString(" [%1]").arg(QString(strg.fileSystemType()));
    }

    m_cmbDrives->addItem(!strg.isReady() ?
                         m_plugin->iconFactory()->fromTheme(QSL("lock")) :
                         m_plugin->iconFactory()->fromTheme(QSL("media-flash")), name_drive, QDir::toNativeSeparators(strg.rootPath()));
  }
}

void FilesystemSidebar::openDrive(int index) {
  QString drive = m_cmbDrives->itemData(index).toString();

  m_fsView->openFolder(drive);
}

void FilesystemSidebar::load() {
  if (m_fsModel == nullptr) {
    m_tabWidget = new QTabWidget(this);
    QWidget* widget_browser = new QWidget(this);
    QVBoxLayout* layout_browser = new QVBoxLayout(widget_browser);

    m_fsModel = new FilesystemModel(m_plugin, widget_browser);
    m_fsView = new FilesystemView(m_fsModel, widget_browser);
    m_lvFavorites = new FavoritesListWidget(m_plugin, m_tabWidget);
    m_txtPath = new BaseTextEdit(widget_browser);
    m_txtPath->setReadOnly(true);

    // Decide the height.
    m_txtPath->setFixedHeight(QFontMetrics(m_txtPath->font()).lineSpacing() * FS_SIDEBAR_PATH_LINES);
    m_tabWidget->setTabPosition(QTabWidget::TabPosition::South);
    layout_browser->setMargin(0);

    // Initialize toolbar.
    QToolBar* tool_bar = new QToolBar(widget_browser);
    QAction* btn_parent = new QAction(m_plugin->iconFactory()->fromTheme(QSL("go-up")),
                                      tr("Go to Parent Folder"), widget_browser);
    QAction* btn_add_favorites = new QAction(m_plugin->iconFactory()->fromTheme(QSL("folder-favorites")),
                                             tr("Add Selected Item to Favorites"), widget_browser);

    connect(btn_parent, &QAction::triggered, m_fsView, &FilesystemView::cdUp);
    connect(btn_add_favorites, &QAction::triggered, this, &FilesystemSidebar::addToFavorites);

    tool_bar->setFixedHeight(26);
    tool_bar->addAction(btn_parent);
    tool_bar->addAction(btn_add_favorites);
    tool_bar->setIconSize(QSize(16, 16));

    // Initialize FS browser.
    m_cmbFilters = new QComboBox(widget_browser);
    m_cmbFilters->setEditable(true);
    m_cmbFilters->setToolTip(tr("Filter Displayed Files"));

    connect(m_cmbFilters, &QComboBox::currentTextChanged, this, [this](const QString& filter) {
      m_fsModel->setNameFilters({filter});
    });

    QStringList fltrs = m_textApp->settings()->syntaxHighlighting()->bareFileFilters();

    std::sort(fltrs.begin(), fltrs.end(), [](const QString& lhs, const QString& rhs) {
      return QString::compare(lhs, rhs, Qt::CaseSensitivity::CaseInsensitive) < 0;
    });

    m_cmbFilters->addItems(fltrs);

    m_cmbDrives = new QComboBox(widget_browser);

    reloadDrives();

    connect(m_cmbDrives, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &FilesystemSidebar::openDrive);

    m_lvFavorites->setIconSize(QSize(12, 12));
    m_lvFavorites->setFrameShape(QFrame::Shape::NoFrame);
    m_fsModel->setNameFilterDisables(false);
    m_fsModel->setFilter(QDir::Filter::Files | QDir::Filter::Hidden | QDir::Filter::System |
                         QDir::Filter::AllDirs | QDir::Filter::NoDotAndDotDot);
    m_fsView->setDragDropMode(QAbstractItemView::DragDropMode::NoDragDrop);
    m_fsView->setIconSize(QSize(12, 12));
    m_fsView->setModel(m_fsModel);
    m_fsView->setFrameShape(QFrame::Shape::NoFrame);
    m_fsModel->setRootPath(QString());

    m_fsView->openFolder(m_plugin->settings()->value(m_settingsSection,
                                                     QL1S("current_folder_") + OS_ID_LOW,
                                                     QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString());

    saveCurrentFolder(m_fsView->currentFolder());

    connect(m_fsView, &QListView::activated, this, &FilesystemSidebar::openFileFolder);
    connect(m_fsView, &FilesystemView::rootIndexChanged, this, [this](const QModelIndex& idx) {
      saveCurrentFolder(idx);
      m_fsView->setFocus();
    });
    connect(m_lvFavorites, &QListWidget::activated, this, &FilesystemSidebar::openFavoriteItem);

    QStringList saved_files = m_plugin->settings()->value(m_settingsSection, QSL("favorites"), QStringList()).toStringList();

    foreach (const QString& file, saved_files) {
      m_lvFavorites->loadFileItem(file);
    }

    m_lvFavorites->sortItems(Qt::SortOrder::AscendingOrder);

    layout_browser->addWidget(m_cmbDrives);
    layout_browser->addWidget(m_txtPath);
    layout_browser->addWidget(tool_bar);
    layout_browser->addWidget(m_fsView, 1);
    layout_browser->addWidget(m_cmbFilters);

    m_tabWidget->addTab(widget_browser, tr("Explorer"));
    m_tabWidget->addTab(m_lvFavorites, tr("Favorites"));

    setWidget(m_tabWidget);
    setFocusProxy(m_fsView);

    BaseSidebar::load();
  }
}

void FilesystemSidebar::saveCurrentFolder(const QString& path) {
  m_txtPath->setPlainText(path);
  m_txtPath->setToolTip(path);

  int index_drive = m_cmbDrives->findData(QDir::toNativeSeparators(QStorageInfo(path).rootPath()));

  m_cmbDrives->blockSignals(true);
  m_cmbDrives->setCurrentIndex(index_drive);
  m_cmbDrives->blockSignals(false);

  m_plugin->settings()->setValue(m_settingsSection, QL1S("current_folder_") + OS_ID_LOW, path);
}

void FilesystemSidebar::saveCurrentFolder(const QModelIndex& idx) {
  auto path = QDir::toNativeSeparators(m_fsView->normalizePath(m_fsModel->filePath(idx)));

  saveCurrentFolder(path);
}

void FilesystemSidebar::addToFavorites() {
  const QFileInfo file_info = m_fsView->selectedFileFolder();

  if (file_info.isFile() || file_info.isDir()) {
    m_lvFavorites->loadFileItem(QDir::toNativeSeparators(file_info.absoluteFilePath()));
    m_lvFavorites->sortItems(Qt::SortOrder::AscendingOrder);
    saveFavorites();
    makeFavoritesVisible();
  }
}

void FilesystemSidebar::openFavoriteItem(const QModelIndex& idx) {
  const auto file_folder = QFileInfo(m_lvFavorites->item(idx.row())->data(Qt::UserRole).toString());

  if (file_folder.isDir()) {
    m_fsView->openFolder(file_folder.absoluteFilePath());
    makeExplorerVisible();
  }
  else {
    emit openFileRequested(file_folder.absoluteFilePath());
  }
}

void FilesystemSidebar::openFileFolder(const QModelIndex& idx) {
  if (m_fsModel->isDir(idx)) {
    // NOTE: This goes from index -> path -> index to properly
    // resolve ".." parent item.
    m_fsView->openFolder(idx);
  }
  else {
    emit openFileRequested(m_fsModel->filePath(idx));
  }
}

void FilesystemSidebar::saveFavorites() const {
  QStringList favorites;

  for (int i = 0; i < m_lvFavorites->count(); i++) {
    favorites.append(m_lvFavorites->item(i)->data(Qt::UserRole).toString());
  }

  m_plugin->settings()->setValue(m_settingsSection, QSL("favorites"), favorites);
}

void FilesystemSidebar::makeExplorerVisible() const {
  m_tabWidget->setCurrentIndex(0);
}

void FilesystemSidebar::makeFavoritesVisible() const {
  m_tabWidget->setCurrentIndex(1);
}
