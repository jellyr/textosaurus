// This file is distributed under GNU GPLv3 license. For full license text, see <project-git-repository-root-folder>/LICENSE.md.

#include "saurus/plugin-system/markdown/markdownplugin.h"

#include "definitions/definitions.h"
#include "saurus/miscellaneous/textapplication.h"
#include "saurus/plugin-system/markdown/markdownsidebar.h"

#include <QAction>

MarkdownPlugin::MarkdownPlugin(QObject* parent) : QObject(parent), m_sidebar(nullptr) {}

QString MarkdownPlugin::name() const {
  return QSL("Markdown Preview");
}

QString MarkdownPlugin::id() const {
  return QSL("io.github.martinrotter.textosaurus.markdownsimple");
}

QList<BaseSidebar*> MarkdownPlugin::sidebars() {
  return QList<BaseSidebar*>() << sidebar();
}

QList<QAction*> MarkdownPlugin::userActions() {
  return QList<QAction*>({
    sidebar()->m_actionRefreshPreview
  });
}

MarkdownSidebar* MarkdownPlugin::sidebar() {
  if (m_sidebar == nullptr) {
    m_sidebar = new MarkdownSidebar(this, nullptr);
  }

  return m_sidebar;
}

void MarkdownPlugin::start(QWidget* main_form_widget, TextApplication* text_app,
                           Settings* settings, IconFactory* icon_factory, WebFactory* web_factory) {

  m_textApp = text_app;
  m_iconFactory = icon_factory;
  m_webFactory = web_factory;
}

void MarkdownPlugin::stop() {}
