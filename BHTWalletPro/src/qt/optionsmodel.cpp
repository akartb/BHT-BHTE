// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Options Model Implementation

#include "optionsmodel.h"

#include <QtCore/QSettings>
#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>

OptionsModel::OptionsModel(QObject* parent)
    : QObject(parent)
{
    Init();
}

OptionsModel::~OptionsModel()
{
}

void OptionsModel::Init()
{
    QSettings settings;

    m_display_unit = settings.value("nDisplayUnit", 0).toInt();
    m_third_party_tx_urls = settings.value("thirdPartyTxUrls", "").toString();
    m_language = settings.value("language", "").toString();
    m_coin_control_features = settings.value("fCoinControlFeatures", false).toBool();
    m_sub_fee_from_amount = settings.value("fSubtractFeeFromAmount", false).toBool();
    m_mask_values = settings.value("fMaskValues", false).toBool();

    m_start_at_startup = settings.value("fStartAtStartup", false).toBool();
    m_hide_tray_icon = settings.value("fHideTrayIcon", false).toBool();
    m_minimize_to_tray = settings.value("fMinimizeToTray", false).toBool();
    m_minimize_on_close = settings.value("fMinimizeOnClose", false).toBool();

    m_proxy_use = settings.value("fUseProxy", false).toBool();
    m_proxy_ip = settings.value("addrProxy", "127.0.0.1").toString();
    m_proxy_port = settings.value("nProxyPort", 9050).toInt();

    m_proxy_use_tor = settings.value("fUseSeparateProxyTor", false).toBool();
    m_proxy_ip_tor = settings.value("addrSeparateProxyTor", "127.0.0.1").toString();
    m_proxy_port_tor = settings.value("nSeparateProxyPortTor", 9050).toInt();

    m_spend_zero_conf_change = settings.value("bSpendZeroConfChange", true).toBool();
    m_listen = settings.value("bListen", true).toBool();
    m_server = settings.value("server", false).toBool();

    m_prune_size = settings.value("nPruneSize", 0).toInt();
    m_database_cache = settings.value("nDatabaseCache", 450).toInt();
    m_threads_script_verif = settings.value("nThreadsScriptVerif", 0).toInt();

    m_font_for_money = QFont("Roboto Mono", 12);
    m_font_for_money.setStyleHint(QFont::Monospace);

    checkAndMigrate();
}

void OptionsModel::Reset()
{
    QSettings settings;
    settings.clear();
    Init();
}

QVariant OptionsModel::getOption(OptionID option, const QString& name) const
{
    Q_UNUSED(name);

    switch (option) {
    case StartAtStartup:
        return m_start_at_startup;
    case HideTrayIcon:
        return m_hide_tray_icon;
    case MinimizeToTray:
        return m_minimize_to_tray;
    case MinimizeOnClose:
        return m_minimize_on_close;
    case DisplayUnit:
        return m_display_unit;
    case ThirdPartyTxUrls:
        return m_third_party_tx_urls;
    case Language:
        return m_language;
    case CoinControlFeatures:
        return m_coin_control_features;
    case SubFeeFromAmount:
        return m_sub_fee_from_amount;
    case MaskValues:
        return m_mask_values;
    case FontForMoney:
        return m_font_for_money;
    case ProxyIP:
        return m_proxy_ip;
    case ProxyPort:
        return m_proxy_port;
    case ProxyUse:
        return m_proxy_use;
    case ProxyUseTor:
        return m_proxy_use_tor;
    case ProxyIPTor:
        return m_proxy_ip_tor;
    case ProxyPortTor:
        return m_proxy_port_tor;
    case SpendZeroConfChange:
        return m_spend_zero_conf_change;
    case Listen:
        return m_listen;
    case Server:
        return m_server;
    case PruneSize:
        return m_prune_size;
    case DatabaseCache:
        return m_database_cache;
    case ThreadsScriptVerif:
        return m_threads_script_verif;
    default:
        return QVariant();
    }
}

bool OptionsModel::setOption(OptionID option, const QVariant& value)
{
    QSettings settings;
    bool successful = true;

    switch (option) {
    case StartAtStartup:
        m_start_at_startup = value.toBool();
        settings.setValue("fStartAtStartup", m_start_at_startup);
        break;
    case HideTrayIcon:
        m_hide_tray_icon = value.toBool();
        settings.setValue("fHideTrayIcon", m_hide_tray_icon);
        break;
    case MinimizeToTray:
        m_minimize_to_tray = value.toBool();
        settings.setValue("fMinimizeToTray", m_minimize_to_tray);
        break;
    case MinimizeOnClose:
        m_minimize_on_close = value.toBool();
        settings.setValue("fMinimizeOnClose", m_minimize_on_close);
        break;
    case DisplayUnit:
        setDisplayUnit(value.toInt());
        break;
    case ThirdPartyTxUrls:
        setThirdPartyTxUrls(value.toString());
        break;
    case Language:
        setLanguage(value.toString());
        break;
    case CoinControlFeatures:
        setCoinControlFeatures(value.toBool());
        break;
    case SubFeeFromAmount:
        m_sub_fee_from_amount = value.toBool();
        settings.setValue("fSubtractFeeFromAmount", m_sub_fee_from_amount);
        break;
    case MaskValues:
        setMaskValues(value.toBool());
        break;
    case FontForMoney:
        setFontForMoney(value.value<QFont>());
        break;
    case ProxyIP:
        m_proxy_ip = value.toString();
        settings.setValue("addrProxy", m_proxy_ip);
        break;
    case ProxyPort:
        m_proxy_port = value.toInt();
        settings.setValue("nProxyPort", m_proxy_port);
        break;
    case ProxyUse:
        m_proxy_use = value.toBool();
        settings.setValue("fUseProxy", m_proxy_use);
        break;
    case ProxyUseTor:
        m_proxy_use_tor = value.toBool();
        settings.setValue("fUseSeparateProxyTor", m_proxy_use_tor);
        break;
    case ProxyIPTor:
        m_proxy_ip_tor = value.toString();
        settings.setValue("addrSeparateProxyTor", m_proxy_ip_tor);
        break;
    case ProxyPortTor:
        m_proxy_port_tor = value.toInt();
        settings.setValue("nSeparateProxyPortTor", m_proxy_port_tor);
        break;
    case SpendZeroConfChange:
        m_spend_zero_conf_change = value.toBool();
        settings.setValue("bSpendZeroConfChange", m_spend_zero_conf_change);
        break;
    case Listen:
        m_listen = value.toBool();
        settings.setValue("bListen", m_listen);
        break;
    case Server:
        m_server = value.toBool();
        settings.setValue("server", m_server);
        break;
    case PruneSize:
        m_prune_size = value.toInt();
        settings.setValue("nPruneSize", m_prune_size);
        break;
    case DatabaseCache:
        m_database_cache = value.toInt();
        settings.setValue("nDatabaseCache", m_database_cache);
        break;
    case ThreadsScriptVerif:
        m_threads_script_verif = value.toInt();
        settings.setValue("nThreadsScriptVerif", m_threads_script_verif);
        break;
    default:
        successful = false;
        break;
    }

    if (successful) {
        Q_EMIT optionChanged(option, value);
    }

    return successful;
}

QString OptionsModel::getOverriddenByCommandLine()
{
    return QString();
}

void OptionsModel::setDisplayUnit(int unit)
{
    if (m_display_unit != unit) {
        m_display_unit = unit;
        QSettings settings;
        settings.setValue("nDisplayUnit", m_display_unit);
        Q_EMIT displayUnitChanged(unit);
    }
}

void OptionsModel::setThirdPartyTxUrls(const QString& urls)
{
    m_third_party_tx_urls = urls;
    QSettings settings;
    settings.setValue("thirdPartyTxUrls", m_third_party_tx_urls);
}

void OptionsModel::setLanguage(const QString& language)
{
    m_language = language;
    QSettings settings;
    settings.setValue("language", m_language);
}

void OptionsModel::setCoinControlFeatures(bool enabled)
{
    if (m_coin_control_features != enabled) {
        m_coin_control_features = enabled;
        QSettings settings;
        settings.setValue("fCoinControlFeatures", m_coin_control_features);
        Q_EMIT coinControlFeaturesChanged(enabled);
    }
}

void OptionsModel::setSubFeeFromAmount(bool enabled)
{
    m_sub_fee_from_amount = enabled;
    QSettings settings;
    settings.setValue("fSubtractFeeFromAmount", m_sub_fee_from_amount);
}

void OptionsModel::setMaskValues(bool mask)
{
    if (m_mask_values != mask) {
        m_mask_values = mask;
        QSettings settings;
        settings.setValue("fMaskValues", m_mask_values);
    }
}

void OptionsModel::setFontForMoney(const QFont& font)
{
    m_font_for_money = font;
    QSettings settings;
    settings.setValue("fontForMoney", font.toString());
    Q_EMIT fontForMoneyChanged(font);
}

void OptionsModel::checkAndMigrate()
{
    QSettings settings;
    int version = settings.value("nSettingsVersion", 0).toInt();
    if (version < 1) {
        settings.setValue("nSettingsVersion", 1);
    }
}
