#ifndef AUTHENTICATIONWINDOW_H
#define AUTHENTICATIONWINDOW_H
#include <DMainWindow>
#include <QLabel>
#include <DSuggestButton>

DWIDGET_USE_NAMESPACE

class AuthenticationWindow : public DMainWindow
{
    Q_OBJECT
public:
    AuthenticationWindow(DMainWindow *prarent = nullptr);

    void setData(QString phoneNumber,QString weChatUnionId,QString acccessToken,QString refreshToken);

Q_SIGNALS:
    void toTellrefreshUserInfo(QString);

private Q_SLOTS:
    void onGetCodeButtonClicked();

    void onSendSmsCodeResult();

    void onLineEditTextChanged(QString text);

    void onNextButtonClicked();

    void onVerifySmsCodeResult();

    void onTimeOut();

private:
    QLabel *m_tipLabel;
    QString m_phoneNumber;
    QString m_acccessToken;
    QString m_refreshToken;
    QString m_weChatUnionId;
    DSuggestButton *m_getCodeButton;
    DSuggestButton *m_nextButton;
    QLineEdit *m_lineEdit;
    QTimer *m_timer;
    int m_second;
};

#endif // AUTHENTICATIONWINDOW_H