// Copyright (c) 2026 BHT Developers
// Distributed under the MIT license
// BHT Wallet Pro - Options Model

#ifndef BHT_WALLET_QT_OPTIONSMODEL_H
#define BHT_WALLET_QT_OPTIONSMODEL_H

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QFont>

class OptionsModel : public QObject
{
    Q_OBJECT

public:
    explicit OptionsModel(QObject* parent = nullptr);
    ~OptionsModel();

    enum OptionID {
        StartAtStartup,
        HideTrayIcon,
        MinimizeToTray,
        MinimizeOnClose,
        DisplayUnit,
        ThirdPartyTxUrls,
        Language,
        CoinControlFeatures,
        SubFeeFromAmount,
        MaskValues,
        FontForMoney,
        ProxyIP,
        ProxyPort,
        ProxyUse,
        ProxyUseTor,
        ProxyIPTor,
        ProxyPortTor,
        SpendZeroConfChange,
        Listen,
        Server,
        PruneSize,
        DatabaseCache,
        ThreadsScriptVerif,
        OptionIDRowCount
    };

    void Init();
    void Reset();

    QVariant getOption(OptionID option, const QString& name = QString()) const;
    bool setOption(OptionID option, const QVariant& value);

    QString getOverriddenByCommandLine();

    int getDisplayUnit() const { return m_display_unit; }
    void setDisplayUnit(int unit);

    QString getThirdPartyTxUrls() const { return m_third_party_tx_urls; }
    void setThirdPartyTxUrls(const QString& urls);

    QString getLanguage() const { return m_language; }
    void setLanguage(const QString& language);

    bool getCoinControlFeatures() const { return m_coin_control_features; }
    void setCoinControlFeatures(bool enabled);

    bool getSubFeeFromAmount() const { return m_sub_fee_from_amount; }
    void setSubFeeFromAmount(bool enabled);

    bool getMaskValues() const { return m_mask_values; }
    void setMaskValues(bool mask);

    QFont getFontForMoney() const { return m_font_for_money; }
    void setFontForMoney(const QFont& font);

    bool getProxyUse() const { return m_proxy_use; }
    QString getProxyIP() const { return m_proxy_ip; }
    int getProxyPort() const { return m_proxy_port; }

    bool getProxyUseTor() const { return m_proxy_use_tor; }
    QString getProxyIPTor() const { return m_proxy_ip_tor; }
    int getProxyPortTor() const { return m_proxy_port_tor; }

    bool getStartAtStartup() const { return m_start_at_startup; }
    bool getHideTrayIcon() const { return m_hide_tray_icon; }
    bool getMinimizeToTray() const { return m_minimize_to_tray; }
    bool getMinimizeOnClose() const { return m_minimize_on_close; }

    bool getPruneEnabled() const { return m_prune_size > 0; }
    int getPruneSize() const { return m_prune_size; }

    int getDatabaseCache() const { return m_database_cache; }
    int getThreadsScriptVerif() const { return m_threads_script_verif; }

Q_SIGNALS:
    void displayUnitChanged(int unit);
    void coinControlFeaturesChanged(bool enabled);
    void fontForMoneyChanged(const QFont& font);
    void optionChanged(OptionID option, const QVariant& value);

private:
    void checkAndMigrate();

    int m_display_unit{0};
    QString m_third_party_tx_urls;
    QString m_language;
    bool m_coin_control_features{false};
    bool m_sub_fee_from_amount{false};
    bool m_mask_values{false};
    QFont m_font_for_money;

    bool m_start_at_startup{false};
    bool m_hide_tray_icon{false};
    bool m_minimize_to_tray{false};
    bool m_minimize_on_close{false};

    bool m_proxy_use{false};
    QString m_proxy_ip{"127.0.0.1"};
    int m_proxy_port{9050};

    bool m_proxy_use_tor{false};
    QString m_proxy_ip_tor{"127.0.0.1"};
    int m_proxy_port_tor{9050};

    bool m_spend_zero_conf_change{true};
    bool m_listen{true};
    bool m_server{false};

    int m_prune_size{0};
    int m_database_cache{450};
    int m_threads_script_verif{0};
};

#endif
