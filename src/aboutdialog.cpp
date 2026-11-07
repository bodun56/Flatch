#include "aboutdialog.h"

#include "appicon.h"
#include "apppaths.h"
#include "uiutils.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const char *const kAuthor = "BoDun";
const char *const kGithubUrl = "https://github.com/bodun56";
const char *const kDzenUrl = "https://dzen.ru/room_bodun";
const char *const kBoostyUrl = "https://boosty.to/room_bodun";
const char *const kDzenDonateUrl = "https://dzen.ru/room_bodun?donate=true";
const char *const kBoostyDonateUrl = "https://boosty.to/room_bodun/donate";

QPushButton *makeUrlButton(const QString &text, const QString &url, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setCursor(Qt::PointingHandCursor);
    QObject::connect(button, &QPushButton::clicked, button, [url] {
        QDesktopServices::openUrl(QUrl(url));
    });
    return button;
}

QLabel *makePathLabel(const QString &path)
{
    auto *label = new QLabel(QDir::toNativeSeparators(path));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    label->setToolTip(QDir::toNativeSeparators(path));
    return label;
}

QLabel *makeHintLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setWordWrap(true);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, palette.color(QPalette::PlaceholderText));
    label->setPalette(palette);
    return label;
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("О Flatch"));
    setWindowIcon(flatchIcon());
    setModal(true);
    fitDialogToScreen(this, QSize(680, 560), QSize(560, 460));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(12);

    // --- Header ---
    auto *header = new QHBoxLayout;
    header->setSpacing(14);

    auto *iconLabel = new QLabel;
    iconLabel->setPixmap(flatchIcon().pixmap(64, 64));
    iconLabel->setFixedSize(64, 64);
    header->addWidget(iconLabel, 0, Qt::AlignTop);

    auto *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(2);

    auto *title = new QLabel(QStringLiteral("Flatch"));
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.8);
    titleFont.setBold(true);
    title->setFont(titleFont);
    titleLayout->addWidget(title);

    auto *version = new QLabel(tr("Версия %1")
                                   .arg(QCoreApplication::applicationVersion()));
    titleLayout->addWidget(version);

    auto *description = new QLabel(
        tr("Менеджер сниппетов в системном трее: обычный и форматированный "
           "текст, файлы и приложения."));
    description->setWordWrap(true);
    titleLayout->addWidget(description);

    header->addLayout(titleLayout, 1);
    mainLayout->addLayout(header);

    // --- Information ---
    auto *infoGroup = new QGroupBox(tr("Информация"));
    auto *infoForm = new QFormLayout(infoGroup);
    infoForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    infoForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *author = new QLabel(QString::fromLatin1(kAuthor));
    QFont authorFont = author->font();
    authorFont.setBold(true);
    author->setFont(authorFont);
    infoForm->addRow(tr("Автор:"), author);

    infoForm->addRow(tr("Версия Qt:"),
                     new QLabel(tr("%1 (сборка %2)")
                                    .arg(QString::fromLatin1(qVersion()),
                                         QStringLiteral(QT_VERSION_STR))));
    infoForm->addRow(tr("База данных:"), makePathLabel(AppPaths::databaseFile()));
    infoForm->addRow(tr("Ключ шифрования:"), makePathLabel(AppPaths::keyFile()));

    mainLayout->addWidget(infoGroup);

    // --- Links ---
    auto *linksGroup = new QGroupBox(tr("Ссылки"));
    auto *linksLayout = new QHBoxLayout(linksGroup);
    linksLayout->setSpacing(8);
    linksLayout->addWidget(makeUrlButton(QStringLiteral("GitHub"),
                                         QString::fromLatin1(kGithubUrl), linksGroup));
    linksLayout->addWidget(makeUrlButton(tr("Дзен"),
                                         QString::fromLatin1(kDzenUrl), linksGroup));
    linksLayout->addWidget(makeUrlButton(QStringLiteral("Boosty"),
                                         QString::fromLatin1(kBoostyUrl), linksGroup));
    linksLayout->addStretch(1);
    mainLayout->addWidget(linksGroup);

    // --- Donations ---
    auto *donateGroup = new QGroupBox(tr("Поддержать проект"));
    auto *donateLayout = new QVBoxLayout(donateGroup);
    donateLayout->setSpacing(8);

    auto *donateButtons = new QHBoxLayout;
    donateButtons->setSpacing(8);
    donateButtons->addWidget(makeUrlButton(tr("Поддержать на Дзене"),
                                           QString::fromLatin1(kDzenDonateUrl), donateGroup));
    donateButtons->addWidget(makeUrlButton(tr("Поддержать на Boosty"),
                                           QString::fromLatin1(kBoostyDonateUrl), donateGroup));
    donateButtons->addStretch(1);
    donateLayout->addLayout(donateButtons);
    donateLayout->addWidget(makeHintLabel(
        tr("Спасибо за поддержку проекта и развитие бесплатных инструментов.")));

    mainLayout->addWidget(donateGroup);
    mainLayout->addStretch(1);

    // --- Buttons ---
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QPushButton *openFolder = buttons->addButton(tr("Открыть папку данных"),
                                                 QDialogButtonBox::ActionRole);
    if (QPushButton *closeButton = buttons->button(QDialogButtonBox::Close))
        closeButton->setText(tr("Закрыть"));

    connect(openFolder, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::dataDir()));
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttons);
}
