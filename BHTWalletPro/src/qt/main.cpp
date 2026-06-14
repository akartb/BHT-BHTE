// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Main Entry Point

#include "bitcoingui.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QTranslator>
#include <QtCore/QLocale>
#include <QtCore/QSettings>
#include <QtCore/QDir>
#include <QtGui/QFont>

#include <iostream>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    app.setOrganizationName("BHT");
    app.setOrganizationDomain("bht.org");
    app.setApplicationName("BHTWalletPro");
    app.setApplicationDisplayName("BHT Wallet Pro");
    app.setApplicationVersion("1.0.0");

    app.setStyle("Fusion");

    QFont font("Segoe UI", 10);
    font.setStyleHint(QFont::SansSerif);
    app.setFont(font);

    QSettings settings;
    QString language = settings.value("language", "en").toString();

    QTranslator translator;
    if (language != "en") {
        QString translationFile = QString("bht_wallet_%1").arg(language);
        if (translator.load(translationFile, ":/translations")) {
            app.installTranslator(&translator);
        }
    }

    OptionsModel optionsModel;
    ClientModel clientModel(&optionsModel);
    WalletModel walletModel;

    BHTWalletGUI window;
    window.setClientModel(&clientModel);
    window.setWalletModel(&walletModel);

    window.resize(1200, 800);
    window.show();

    return app.exec();
}
