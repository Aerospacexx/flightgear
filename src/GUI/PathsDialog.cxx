#include "PathsDialog.hxx"
#include "ui_PathsDialog.h"

#include <QSettings>
#include <QFileDialog>

#include "CatalogListModel.hxx"
#include "AddCatalogDialog.hxx"

#include <Main/options.hxx>

PathsDialog::PathsDialog(QWidget *parent, simgear::pkg::RootRef root) :
    QDialog(parent),
    m_ui(new Ui::PathsDialog),
    m_packageRoot(root)
{
    m_ui->setupUi(this);
#if defined(CATALOG_SUPPORT)
    m_catalogsModel = new CatalogListModel(this, m_packageRoot);
    m_ui->catalogsList->setModel(m_catalogsModel);

    connect(m_ui->addCatalog, &QToolButton::clicked,
            this, &PathsDialog::onAddCatalog);
    connect(m_ui->removeCatalog, &QToolButton::clicked,
            this, &PathsDialog::onRemoveCatalog);
#endif
    connect(m_ui->addSceneryPath, &QToolButton::clicked,
            this, &PathsDialog::onAddSceneryPath);
    connect(m_ui->removeSceneryPath, &QToolButton::clicked,
            this, &PathsDialog::onRemoveSceneryPath);

    connect(m_ui->addAircraftPath, &QToolButton::clicked,
            this, &PathsDialog::onAddAircraftPath);
    connect(m_ui->removeAircraftPath, &QToolButton::clicked,
            this, &PathsDialog::onRemoveAircraftPath);

    connect(m_ui->changeDownloadDir, &QPushButton::clicked,
            this, &PathsDialog::onChangeDownloadDir);

    connect(m_ui->clearDownloadDir, &QPushButton::clicked,
            this, &PathsDialog::onClearDownloadDir);

    QSettings settings;
            
    QStringList sceneryPaths = settings.value("scenery-paths").toStringList();
    m_ui->sceneryPathsList->addItems(sceneryPaths);

    QStringList aircraftPaths = settings.value("aircraft-paths").toStringList();
    m_ui->aircraftPathsList->addItems(aircraftPaths);

    QVariant downloadDir = settings.value("download-dir");
    if (downloadDir.isValid()) {
        m_downloadDir = downloadDir.toString();
    }

    updateUi();
}

PathsDialog::~PathsDialog()
{
    delete m_ui;
}

void PathsDialog::accept()
{
    QSettings settings;
    QStringList paths;
    for (int i=0; i<m_ui->sceneryPathsList->count(); ++i) {
        paths.append(m_ui->sceneryPathsList->item(i)->text());
    }

    settings.setValue("scenery-paths", paths);
    paths.clear();

    for (int i=0; i<m_ui->aircraftPathsList->count(); ++i) {
        paths.append(m_ui->aircraftPathsList->item(i)->text());
    }

    settings.setValue("aircraft-paths", paths);

    if (m_downloadDir.isEmpty()) {
        settings.remove("download-dir");
    } else {
        settings.setValue("download-dir", m_downloadDir);
    }
    
    QDialog::accept();
}

void PathsDialog::onAddSceneryPath()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Choose scenery folder"));
    if (!path.isEmpty()) {
        m_ui->sceneryPathsList->addItem(path);
    }
}

void PathsDialog::onRemoveSceneryPath()
{
    if (m_ui->sceneryPathsList->currentItem()) {
        delete m_ui->sceneryPathsList->currentItem();
    }
}

void PathsDialog::onAddAircraftPath()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Choose aircraft folder"));
    if (!path.isEmpty()) {
        m_ui->aircraftPathsList->addItem(path);
    }
}

void PathsDialog::onRemoveAircraftPath()
{
    if (m_ui->aircraftPathsList->currentItem()) {
        delete m_ui->aircraftPathsList->currentItem();
    }
}

void PathsDialog::onAddCatalog()
{
#if defined(CATALOG_SUPPORT)
    AddCatalogDialog* dlg = new AddCatalogDialog(this, m_packageRoot);
    dlg->exec();
    if (dlg->result() == QDialog::Accepted) {
        m_catalogsModel->refresh();
    }
#endif
}

void PathsDialog::onRemoveCatalog()
{
    
}

void PathsDialog::onChangeDownloadDir()
{
    QString path = QFileDialog::getExistingDirectory(this,
                                                     tr("Choose downloads folder"),
                                                     m_downloadDir);
    if (!path.isEmpty()) {
        m_downloadDir = path;
        updateUi();
    }
}

void PathsDialog::onClearDownloadDir()
{
    // does this need an 'are you sure'?
    m_downloadDir.clear();
    updateUi();
}

void PathsDialog::updateUi()
{
    QString s = m_downloadDir;
    if (s.isEmpty()) {
        s = QString::fromStdString(flightgear::defaultDownloadDir());
        s.append(tr(" (default)"));
        m_ui->clearDownloadDir->setEnabled(false);
    } else {
        m_ui->clearDownloadDir->setEnabled(true);
    }

    QString m = tr("Download location: %1").arg(s);
    m_ui->downloadLocation->setText(m);
}
