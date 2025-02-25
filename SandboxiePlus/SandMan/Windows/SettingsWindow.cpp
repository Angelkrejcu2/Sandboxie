#include "stdafx.h"
#include "SettingsWindow.h"
#include "SandMan.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/Common.h"
#include "Helpers/WinAdmin.h"
#include "../QSbieAPI/Sandboxie/SbieTemplates.h"
#include "../QSbieAPI/SbieUtils.h"
#include "OptionsWindow.h"


QSize CustomTabStyle::sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size, const QWidget* widget) const {
	QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
	if (type == QStyle::CT_TabBarTab) {
		s.transpose();
		if(theGUI->m_DarkTheme)
			s.setHeight(s.height() * 13 / 10);
		else
			s.setHeight(s.height() * 15 / 10);
		s.setWidth(s.width() * 11 / 10); // for the the icon
	}
	return s;
}

void CustomTabStyle::drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const {
	if (element == CE_TabBarTabLabel) {
		if (const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
			QStyleOptionTab opt(*tab);
			opt.shape = QTabBar::RoundedNorth;
			//opt.iconSize = QSize(32, 32);
			opt.iconSize = QSize(24, 24);
			QProxyStyle::drawControl(element, &opt, painter, widget);
			return;
		}
	}
	QProxyStyle::drawControl(element, option, painter, widget);
}


int CSettingsWindow__Chk2Int(Qt::CheckState state)
{
	switch (state) {
	case Qt::Unchecked: return 0;
	case Qt::Checked: return 1;
	default:
	case Qt::PartiallyChecked: return 2;
	}
}

Qt::CheckState CSettingsWindow__Int2Chk(int state)
{
	switch (state) {
	case 0: return Qt::Unchecked;
	case 1: return Qt::Checked;
	default:
	case 2: return Qt::PartiallyChecked;
	}
}

quint32 g_FeatureFlags = 0;

QByteArray g_Certificate;
SCertInfo g_CertInfo = { 0 };

CSettingsWindow::CSettingsWindow(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	this->setWindowTitle(tr("Sandboxie Plus - Settings"));

	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	setWindowFlags(flags);

	bool bAlwaysOnTop = theConf->GetBool("Options/AlwaysOnTop", false);
	this->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);


	ui.tabs->setTabPosition(QTabWidget::West);
	ui.tabs->tabBar()->setStyle(new CustomTabStyle(ui.tabs->tabBar()->style()));

	ui.tabs->setTabIcon(eOptions, CSandMan::GetIcon("Options"));
	ui.tabs->setTabIcon(eShell, CSandMan::GetIcon("Shell"));
	ui.tabs->setTabIcon(eGuiConfig, CSandMan::GetIcon("GUI"));
	ui.tabs->setTabIcon(eAdvanced, CSandMan::GetIcon("Advanced"));
	ui.tabs->setTabIcon(eProgCtrl, CSandMan::GetIcon("Ampel"));
	ui.tabs->setTabIcon(eConfigLock, CSandMan::GetIcon("Lock"));
	ui.tabs->setTabIcon(eSoftCompat, CSandMan::GetIcon("Compatibility"));
	ui.tabs->setTabIcon(eEditIni, CSandMan::GetIcon("EditIni"));
	ui.tabs->setTabIcon(eSupport, CSandMan::GetIcon("Support"));


	ui.tabs->setCurrentIndex(0);

	ui.uiLang->addItem(tr("Auto Detection"), "");
	ui.uiLang->addItem(tr("No Translation"), "native");
	QDir langDir(QApplication::applicationDirPath() + "/translations/");
	foreach(const QString& langFile, langDir.entryList(QStringList("sandman_*.qm"), QDir::Files))
	{
		QString Code = langFile.mid(8, langFile.length() - 8 - 3);
		QLocale Locale(Code);
		QString Lang = Locale.nativeLanguageName();
		ui.uiLang->addItem(Lang, Code);
	}
	ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("Options/UiLanguage")));

	ui.cmbSysTray->addItem(tr("Don't show any icon"));
	ui.cmbSysTray->addItem(tr("Show Plus icon"));
	ui.cmbSysTray->addItem(tr("Show Classic icon"));

	ui.cmbTrayBoxes->addItem(tr("All Boxes"));
	ui.cmbTrayBoxes->addItem(tr("Active + Pinned"));
	ui.cmbTrayBoxes->addItem(tr("Pinned Only"));

	ui.cmbOnClose->addItem(tr("Close to Tray"), "ToTray");
	ui.cmbOnClose->addItem(tr("Prompt before Close"), "Prompt");
	ui.cmbOnClose->addItem(tr("Close"), "Close");


	ui.cmbDPI->addItem(tr("None"), 0);
	ui.cmbDPI->addItem(tr("Native"), 1);
	ui.cmbDPI->addItem(tr("Qt"), 2);

	int FontScales[] = { 75,100,125,150,175,200,225,250,275,300,350,400, 0 };
	for(int* pFontScales = FontScales; *pFontScales != 0; pFontScales++)
		ui.cmbFontScale->addItem(tr("%1").arg(*pFontScales), *pFontScales);

	QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", QSettings::NativeFormat);
	if (settings.value("CurrentBuild").toInt() >= 22000) { // Windows 11
		QCheckBox* SecretCheckBox = new CSecretCheckBox(ui.chkShellMenu->text());
		((QGridLayout*)((QWidget*)ui.chkShellMenu->parent())->layout())->replaceWidget(ui.chkShellMenu, SecretCheckBox);
		ui.chkShellMenu->deleteLater();
		ui.chkShellMenu = SecretCheckBox;
	}

	LoadSettings();

	connect(ui.uiLang, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChangeGUI()));

	connect(ui.cmbDPI, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.chkDarkTheme, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.chkBackground, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.chkLargeIcons, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.chkNoIcons, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	//connect(ui.chkOptTree, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.chkColorIcons, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.cmbFontScale, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.cmbFontScale, SIGNAL(currentTextChanged(const QString&)), this, SLOT(OnChangeGUI()));


	m_bRebuildUI = false;

	connect(ui.cmbSysTray, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChange()));
	connect(ui.cmbTrayBoxes, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChange()));
	connect(ui.chkCompactTray, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.cmbOnClose, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChange()));

	m_FeaturesChanged = false;
	connect(ui.chkWFP, SIGNAL(stateChanged(int)), this, SLOT(OnFeaturesChanged()));
	connect(ui.chkObjCb, SIGNAL(stateChanged(int)), this, SLOT(OnFeaturesChanged()));
	//connect(ui.chkWin32k, SIGNAL(stateChanged(int)), this, SLOT(OnFeaturesChanged()));
	
	m_WarnProgsChanged = false;

	connect(ui.chkPassRequired, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));
	connect(ui.btnSetPassword, SIGNAL(clicked(bool)), this, SLOT(OnSetPassword()));

	connect(ui.chkStartBlock, SIGNAL(stateChanged(int)), this, SLOT(OnWarnChanged()));

	connect(ui.chkStartBlockMsg, SIGNAL(stateChanged(int)), this, SLOT(OnWarnChanged()));
	connect(ui.btnAddWarnProg, SIGNAL(clicked(bool)), this, SLOT(OnAddWarnProg()));
	connect(ui.btnAddWarnFolder, SIGNAL(clicked(bool)), this, SLOT(OnAddWarnFolder()));
	connect(ui.btnDelWarnProg, SIGNAL(clicked(bool)), this, SLOT(OnDelWarnProg()));

	connect(ui.btnBrowse, SIGNAL(clicked(bool)), this, SLOT(OnBrowse()));

	connect(ui.chkAutoRoot, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));


	connect(ui.btnAddCompat, SIGNAL(clicked(bool)), this, SLOT(OnAddCompat()));
	connect(ui.btnDelCompat, SIGNAL(clicked(bool)), this, SLOT(OnDelCompat()));
	m_CompatLoaded = 0;
	m_CompatChanged = false;
	ui.chkNoCompat->setChecked(!theConf->GetBool("Options/AutoRunSoftCompat", true));

	connect(ui.treeCompat, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnTemplateClicked(QTreeWidgetItem*, int)));
	connect(ui.treeCompat, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnTemplateDoubleClicked(QTreeWidgetItem*, int)));

	connect(ui.lblSupport, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
	connect(ui.lblSupportCert, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
	connect(ui.lblCertExp, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));

	m_CertChanged = false;
	connect(ui.txtCertificate, SIGNAL(textChanged()), this, SLOT(CertChanged()));
	connect(theGUI, SIGNAL(CertUpdated()), this, SLOT(UpdateCert()));

	ui.txtCertificate->setPlaceholderText(
		"NAME: User Name\n"
		"LEVEL: ULTIMATE\n"
		"DATE: dd.mm.yyyy\n"
		"UPDATEKEY: 00000000000000000000000000000000\n"
		"SIGNATURE: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="
	);

	connect(ui.tabs, SIGNAL(currentChanged(int)), this, SLOT(OnTab()));

	// edit
	connect(ui.btnEditIni, SIGNAL(clicked(bool)), this, SLOT(OnEditIni()));
	connect(ui.btnSaveIni, SIGNAL(clicked(bool)), this, SLOT(OnSaveIni()));
	connect(ui.btnCancelEdit, SIGNAL(clicked(bool)), this, SLOT(OnCancelEdit()));
	//connect(ui.txtIniSection, SIGNAL(textChanged()), this, SLOT(OnOptChanged()));

	connect(ui.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked(bool)), this, SLOT(ok()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	restoreGeometry(theConf->GetBlob("SettingsWindow/Window_Geometry"));
}

CSettingsWindow::~CSettingsWindow()
{
	theConf->SetBlob("SettingsWindow/Window_Geometry",saveGeometry());
}

void CSettingsWindow::showTab(int Tab, bool bExclusive)
{
	if(Tab == CSettingsWindow::eSoftCompat)
		m_CompatLoaded = 2;
	else if(Tab == CSettingsWindow::eSupport)
		ui.chkNoCheck->setVisible(true);

	if(bExclusive)
		ui.tabs->tabBar()->setVisible(false);

	ui.tabs->setCurrentIndex(Tab);
	SafeShow(this);
}

void CSettingsWindow::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

Qt::CheckState CSettingsWindow__IsContextMenu()
{
	QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\PackagedCom\\Package", QSettings::NativeFormat);
	foreach(const QString & Key, settings.childGroups()) {
		if (Key.indexOf("SandboxieShell") == 0)
			return Qt::Checked;
	}

	QString cmd = CSbieUtils::GetContextMenuStartCmd();
	if (cmd.contains("SandMan.exe", Qt::CaseInsensitive)) 
		return Qt::Checked; // set up and sandman
	if (!cmd.isEmpty()) // ... probably sbiectrl.exe
		return Qt::PartiallyChecked; 
	return Qt::Unchecked; // not set up
}

void CSettingsWindow__AddContextMenu(bool bAlwaysClassic)
{
	QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", QSettings::NativeFormat);
	if (settings.value("CurrentBuild").toInt() >= 22000 && !bAlwaysClassic) // Windows 11
	{
		QProcess Proc;
		Proc.execute("rundll32.exe", QStringList() << QCoreApplication::applicationDirPath().replace("/", "\\") + "\\SbieShellExt.dll,RegisterPackage");
		Proc.waitForFinished();
		return;
	}

	CSbieUtils::AddContextMenu(QApplication::applicationDirPath().replace("/", "\\") + "\\SandMan.exe",
		CSettingsWindow::tr("Run &Sandboxed"), //CSettingsWindow::tr("Explore &Sandboxed"),
			QApplication::applicationDirPath().replace("/", "\\") + "\\Start.exe");
}

void CSettingsWindow__RemoveContextMenu()
{
	QSettings settings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", QSettings::NativeFormat);
	if (settings.value("CurrentBuild").toInt() >= 22000) // Windows 11
	{
		QProcess Proc;
		Proc.execute("rundll32.exe", QStringList() << QCoreApplication::applicationDirPath().replace("/", "\\") + "\\SbieShellExt.dll,RemovePackage");
		Proc.waitForFinished();
	}

	CSbieUtils::RemoveContextMenu();
}

void CSettingsWindow__AddBrowserIcon()
{
	QString Path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).replace("/", "\\");
	Path += "\\" + CSettingsWindow::tr("Sandboxed Web Browser") + ".lnk";
	CSbieUtils::CreateShortcut(theAPI, Path, "", "", "default_browser");
}

void CSettingsWindow::LoadSettings()
{
	ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("Options/UiLanguage")));

	ui.chkAutoStart->setChecked(IsAutorunEnabled());
	if (theAPI->IsConnected()) {
		if (theAPI->GetUserSettings()->GetBool("SbieCtrl_EnableAutoStart", true)) {
			if (theAPI->GetUserSettings()->GetText("SbieCtrl_AutoStartAgent", "") != "SandMan.exe")
				ui.chkSvcStart->setCheckState(Qt::PartiallyChecked);
			else
				ui.chkSvcStart->setChecked(true);
		}
		else
			ui.chkSvcStart->setChecked(false);
	}
	else {
		ui.chkSvcStart->setEnabled(false);
	}

	ui.chkShellMenu->setCheckState(CSettingsWindow__IsContextMenu());
	ui.chkShellMenu2->setChecked(CSbieUtils::HasContextMenu2());
	ui.chkAlwaysDefault->setChecked(theConf->GetBool("Options/RunInDefaultBox", false));

	ui.cmbDPI->setCurrentIndex(theConf->GetInt("Options/DPIScaling", 1));

	ui.chkDarkTheme->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/UseDarkTheme", 2)));
	ui.chkBackground->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/UseBackground", 2)));
	ui.chkLargeIcons->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/LargeIcons", 2)));
	ui.chkNoIcons->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/NoIcons", 2)));
	ui.chkOptTree->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/OptionTree", 2)));
	ui.chkColorIcons->setChecked(theConf->GetBool("Options/ColorBoxIcons", false));

	//ui.cmbFontScale->setCurrentIndex(ui.cmbFontScale->findData(theConf->GetInt("Options/FontScaling", 100)));
	ui.cmbFontScale->setCurrentText(QString::number(theConf->GetInt("Options/FontScaling", 100)));

	ui.chkNotifications->setChecked(theConf->GetBool("Options/ShowNotifications", true));

	ui.chkSandboxUrls->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/OpenUrlsSandboxed", 2)));

	ui.chkShowRecovery->setChecked(theConf->GetBool("Options/ShowRecovery", false));
	ui.chkNotifyRecovery->setChecked(!theConf->GetBool("Options/InstantRecovery", true));
	ui.chkAsyncBoxOps->setChecked(theConf->GetBool("Options/UseAsyncBoxOps", false));

	ui.chkPanic->setChecked(theConf->GetBool("Options/EnablePanicKey", false));
	ui.keyPanic->setKeySequence(QKeySequence(theConf->GetString("Options/PanicKeySequence", "Shift+Pause")));

	ui.chkMonitorSize->setChecked(theConf->GetBool("Options/WatchBoxSize", false));

	ui.chkWatchConfig->setChecked(theConf->GetBool("Options/WatchIni", true));

	
	ui.cmbSysTray->setCurrentIndex(theConf->GetInt("Options/SysTrayIcon", 1));
	ui.cmbTrayBoxes->setCurrentIndex(theConf->GetInt("Options/SysTrayFilter", 0));
	ui.chkCompactTray->setChecked(theConf->GetBool("Options/CompactTray", false));
	ui.chkBoxOpsNotify->setChecked(theConf->GetBool("Options/AutoBoxOpsNotify", false));
	ui.cmbOnClose->setCurrentIndex(ui.cmbOnClose->findData(theConf->GetString("Options/OnClose", "ToTray")));


	if (theAPI->IsConnected())
	{
		QString FileRootPath_Default = "\\??\\%SystemDrive%\\Sandbox\\%USER%\\%SANDBOX%";
		QString KeyRootPath_Default  = "\\REGISTRY\\USER\\Sandbox_%USER%_%SANDBOX%";
		QString IpcRootPath_Default  = "\\Sandbox\\%USER%\\%SANDBOX%\\Session_%SESSION%";

		ui.fileRoot->setText(theAPI->GetGlobalSettings()->GetText("FileRootPath", FileRootPath_Default));
		ui.chkSeparateUserFolders->setChecked(theAPI->GetGlobalSettings()->GetBool("SeparateUserFolders", true));
		ui.regRoot->setText(theAPI->GetGlobalSettings()->GetText("KeyRootPath", KeyRootPath_Default));
		ui.ipcRoot->setText(theAPI->GetGlobalSettings()->GetText("IpcRootPath", IpcRootPath_Default));

		ui.chkWFP->setChecked(theAPI->GetGlobalSettings()->GetBool("NetworkEnableWFP", false));
		ui.chkObjCb->setChecked(theAPI->GetGlobalSettings()->GetBool("EnableObjectFiltering", true));
		ui.chkWin32k->setChecked(theAPI->GetGlobalSettings()->GetBool("EnableWin32kHooks", true));
		ui.chkSbieLogon->setChecked(theAPI->GetGlobalSettings()->GetBool("SandboxieLogon", true));

		ui.chkAdminOnly->setChecked(theAPI->GetGlobalSettings()->GetBool("EditAdminOnly", false));
		ui.chkPassRequired->setChecked(!theAPI->GetGlobalSettings()->GetText("EditPassword", "").isEmpty());
		ui.chkAdminOnlyFP->setChecked(theAPI->GetGlobalSettings()->GetBool("ForceDisableAdminOnly", false));
		ui.chkClearPass->setChecked(theAPI->GetGlobalSettings()->GetBool("ForgetPassword", false));

		ui.chkStartBlock->setChecked(theAPI->GetGlobalSettings()->GetBool("StartRunAlertDenied", false));
		ui.chkStartBlockMsg->setChecked(theAPI->GetGlobalSettings()->GetBool("NotifyStartRunAccessDenied", true));
		
		ui.treeWarnProgs->clear();

		foreach(const QString& Value, theAPI->GetGlobalSettings()->GetTextList("AlertProcess", false))
			AddWarnEntry(Value, 1);
		
		foreach(const QString& Value, theAPI->GetGlobalSettings()->GetTextList("AlertFolder", false))
			AddWarnEntry(Value, 2);
	}
	else
	{
		ui.fileRoot->setEnabled(false);
		ui.chkSeparateUserFolders->setEnabled(false);
		ui.chkAutoRoot->setEnabled(false);
		ui.chkWFP->setEnabled(false);
		ui.chkObjCb->setEnabled(false);
		ui.chkWin32k->setEnabled(false);
		ui.chkSbieLogon->setEnabled(false);
		ui.regRoot->setEnabled(false);
		ui.ipcRoot->setEnabled(false);
		ui.chkAdminOnly->setEnabled(false);
		ui.chkPassRequired->setEnabled(false);
		ui.chkAdminOnlyFP->setEnabled(false);
		ui.chkClearPass->setEnabled(false);
		ui.btnSetPassword->setEnabled(false);
		ui.treeWarnProgs->setEnabled(false);
		ui.btnAddWarnProg->setEnabled(false);
		ui.btnDelWarnProg->setEnabled(false);
		ui.treeCompat->setEnabled(false);
		ui.btnAddCompat->setEnabled(false);
		ui.btnDelCompat->setEnabled(false);
	}


	if (theGUI->IsFullyPortable())
		ui.chkAutoRoot->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/PortableRootDir", 2)));
	else
		ui.chkAutoRoot->setVisible(false);

	UpdateCert();

	ui.chkAutoUpdate->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/CheckForUpdates", 2)));
	ui.chkAutoDownload->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/DownloadUpdates", 0)));
	//ui.chkAutoInstall->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/InstallUpdates", 0)));
	ui.chkAutoInstall->setVisible(false); // todo implement smart auto updater

	ui.chkNoCheck->setChecked(theConf->GetBool("Options/NoSupportCheck", false));
	if(ui.chkNoCheck->isCheckable() && !g_CertInfo.expired)
		ui.chkNoCheck->setVisible(false); // hide if not relevant

	OnChange();
}

void CSettingsWindow::UpdateCert()
{
	ui.lblCertExp->setVisible(false);
	if (!g_Certificate.isEmpty()) 
	{
		ui.txtCertificate->setPlainText(g_Certificate);
		//ui.lblSupport->setVisible(false);

		QPalette palette = QApplication::palette();
		if (theGUI->m_DarkTheme)
			palette.setColor(QPalette::Text, Qt::black);
		if (g_CertInfo.expired) {
			palette.setColor(QPalette::Base, QColor(255, 255, 192));
			ui.lblCertExp->setText(tr("This supporter certificate has expired, please <a href=\"sbie://update/cert\">get an updated certificate</a>."));
			ui.lblCertExp->setVisible(true);
		}
		else {
			if (g_CertInfo.about_to_expire) {
				ui.lblCertExp->setText(tr("This supporter certificate will <font color='red'>expire in %1 days</font>, please <a href=\"sbie://update/cert\">get an updated certificate</a>.").arg(g_CertInfo.expirers_in_sec / (60*60*24)));
				ui.lblCertExp->setVisible(true);
			}
			palette.setColor(QPalette::Base, QColor(192, 255, 192));
		}
		ui.txtCertificate->setPalette(palette);
	}
}

void CSettingsWindow::WriteAdvancedCheck(QCheckBox* pCheck, const QString& Name, const QString& OnValue, const QString& OffValue)
{
	//if (pCheck->checkState() == Qt::PartiallyChecked)
	//	return;

	if (!pCheck->isEnabled())
		return;

	SB_STATUS Status;
	if (pCheck->checkState() == Qt::Checked)
	{
		if(!OnValue.isEmpty())
			Status = theAPI->GetGlobalSettings()->SetText(Name, OnValue);
		else
			Status = theAPI->GetGlobalSettings()->DelValue(Name);
	}
	else if (pCheck->checkState() == Qt::Unchecked)
	{
		if (!OffValue.isEmpty())
			Status = theAPI->GetGlobalSettings()->SetText(Name, OffValue);
		else
			Status = theAPI->GetGlobalSettings()->DelValue(Name);
	}

	if (!Status)
		throw Status;
}

void CSettingsWindow::WriteText(const QString& Name, const QString& Value)
{
	SB_STATUS Status;
	if(Value.isEmpty())
		Status = theAPI->GetGlobalSettings()->DelValue(Name);
	else
		Status = theAPI->GetGlobalSettings()->SetText(Name, Value);
	if (!Status)
		throw Status;
}

void CSettingsWindow::WriteTextList(const QString& Setting, const QStringList& List)
{
	SB_STATUS Status = theAPI->GetGlobalSettings()->UpdateTextList(Setting, List, false);
	if (!Status)
		throw Status;
}

void CSettingsWindow::SaveSettings()
{
	theConf->SetValue("Options/UiLanguage", ui.uiLang->currentData());

	theConf->SetValue("Options/DPIScaling", ui.cmbDPI->currentData());

	theConf->SetValue("Options/UseDarkTheme", CSettingsWindow__Chk2Int(ui.chkDarkTheme->checkState()));
	theConf->SetValue("Options/UseBackground", CSettingsWindow__Chk2Int(ui.chkBackground->checkState()));
	theConf->SetValue("Options/LargeIcons", CSettingsWindow__Chk2Int(ui.chkLargeIcons->checkState()));
	theConf->SetValue("Options/NoIcons", CSettingsWindow__Chk2Int(ui.chkNoIcons->checkState()));
	theConf->SetValue("Options/OptionTree", CSettingsWindow__Chk2Int(ui.chkOptTree->checkState()));
	theConf->SetValue("Options/ColorBoxIcons", ui.chkColorIcons->isChecked());

	int Scaling = ui.cmbFontScale->currentText().toInt();
	if (Scaling < 75)
		Scaling = 75;
	else if (Scaling > 500)
		Scaling = 500;
	theConf->SetValue("Options/FontScaling", Scaling);

	AutorunEnable(ui.chkAutoStart->isChecked());

	if (theAPI->IsConnected()) {
		if (ui.chkSvcStart->checkState() == Qt::Checked) {
			theAPI->GetUserSettings()->SetBool("SbieCtrl_EnableAutoStart", true);
			theAPI->GetUserSettings()->SetText("SbieCtrl_AutoStartAgent", "SandMan.exe");
		}
		else if (ui.chkSvcStart->checkState() == Qt::Unchecked)
			theAPI->GetUserSettings()->SetBool("SbieCtrl_EnableAutoStart", false);
	}

	if (ui.chkShellMenu->checkState() != CSettingsWindow__IsContextMenu())
	{
		if (ui.chkShellMenu->isChecked()) {
			CSecretCheckBox* SecretCheckBox = qobject_cast<CSecretCheckBox*>(ui.chkShellMenu);
			CSettingsWindow__AddContextMenu(SecretCheckBox && SecretCheckBox->IsSecretSet());
		}
		else
			CSettingsWindow__RemoveContextMenu();
	}

	if (ui.chkShellMenu2->isChecked() != CSbieUtils::HasContextMenu2()) {
		if (ui.chkShellMenu2->isChecked()) {
			CSbieUtils::AddContextMenu2(QApplication::applicationDirPath().replace("/", "\\") + "\\Start.exe",
				tr("Run &Un-Sandboxed"),
				QApplication::applicationDirPath().replace("/", "\\") + "\\Start.exe");
		} else
			CSbieUtils::RemoveContextMenu2();
	}

	theConf->SetValue("Options/RunInDefaultBox", ui.chkAlwaysDefault->isChecked());

	theConf->SetValue("Options/ShowNotifications", ui.chkNotifications->isChecked());

	theConf->SetValue("Options/OpenUrlsSandboxed", CSettingsWindow__Chk2Int(ui.chkSandboxUrls->checkState()));

	theConf->SetValue("Options/ShowRecovery", ui.chkShowRecovery->isChecked());
	theConf->SetValue("Options/InstantRecovery", !ui.chkNotifyRecovery->isChecked());
	theConf->SetValue("Options/UseAsyncBoxOps", ui.chkAsyncBoxOps->isChecked());

	theConf->SetValue("Options/EnablePanicKey", ui.chkPanic->isChecked());
	theConf->SetValue("Options/PanicKeySequence", ui.keyPanic->keySequence().toString());
	
	theConf->SetValue("Options/WatchBoxSize", ui.chkMonitorSize->isChecked());

	theConf->SetValue("Options/WatchIni", ui.chkWatchConfig->isChecked());

	theConf->SetValue("Options/SysTrayIcon", ui.cmbSysTray->currentIndex());
	theConf->SetValue("Options/SysTrayFilter", ui.cmbTrayBoxes->currentIndex());
	theConf->SetValue("Options/CompactTray", ui.chkCompactTray->isChecked());
	theConf->SetValue("Options/AutoBoxOpsNotify", ui.chkBoxOpsNotify->isChecked());
	theConf->SetValue("Options/OnClose", ui.cmbOnClose->currentData());


	if (theAPI->IsConnected())
	{
		try
		{
			WriteText("FileRootPath", ui.fileRoot->text()); //ui.fileRoot->setText("\\??\\%SystemDrive%\\Sandbox\\%USER%\\%SANDBOX%");
			WriteAdvancedCheck(ui.chkSeparateUserFolders, "SeparateUserFolders", "", "n");
			WriteText("KeyRootPath", ui.regRoot->text()); //ui.regRoot->setText("\\REGISTRY\\USER\\Sandbox_%USER%_%SANDBOX%");
			WriteText("IpcRootPath", ui.ipcRoot->text()); //ui.ipcRoot->setText("\\Sandbox\\%USER%\\%SANDBOX%\\Session_%SESSION%");

			WriteAdvancedCheck(ui.chkWFP, "NetworkEnableWFP", "y", "");
			WriteAdvancedCheck(ui.chkObjCb, "EnableObjectFiltering", "", "n");
			WriteAdvancedCheck(ui.chkWin32k, "EnableWin32kHooks", "", "n");
			WriteAdvancedCheck(ui.chkSbieLogon, "SandboxieLogon", "", "n");

			if (m_FeaturesChanged) {
				m_FeaturesChanged = false;
				theAPI->ReloadConfig(true);
			}

			WriteAdvancedCheck(ui.chkAdminOnly, "EditAdminOnly", "y", "");

			bool isPassSet = !theAPI->GetGlobalSettings()->GetText("EditPassword", "").isEmpty();
			if (ui.chkPassRequired->isChecked())
			{
				if (!isPassSet && m_NewPassword.isEmpty())
					OnSetPassword(); // request password entry if it wasn't entered already
				if (!m_NewPassword.isEmpty()) {
					theAPI->LockConfig(m_NewPassword); // set new/changed password
					m_NewPassword.clear();
				}
			}
			else if (isPassSet)
				theAPI->LockConfig(QString()); // clear password

			WriteAdvancedCheck(ui.chkAdminOnlyFP, "ForceDisableAdminOnly", "y", "");
			WriteAdvancedCheck(ui.chkClearPass, "ForgetPassword", "y", "");

			if (m_WarnProgsChanged)
			{
				WriteAdvancedCheck(ui.chkStartBlock, "StartRunAlertDenied", "y", "");
				WriteAdvancedCheck(ui.chkStartBlockMsg, "NotifyStartRunAccessDenied", "", "n");

				QStringList AlertProcess;
				QStringList AlertFolder;
				for (int i = 0; i < ui.treeWarnProgs->topLevelItemCount(); i++)
				{
					QTreeWidgetItem* pItem = ui.treeWarnProgs->topLevelItem(i);
					int Type = pItem->data(0, Qt::UserRole).toInt();
					switch (Type)
					{
					case 1:	AlertProcess.append(pItem->data(1, Qt::UserRole).toString()); break;
					case 2: AlertFolder.append(pItem->data(1, Qt::UserRole).toString()); break;
					}
				}

				WriteTextList("AlertProcess", AlertProcess);
				WriteTextList("AlertFolder", AlertFolder);
				m_WarnProgsChanged = false;
			}

			if (m_CompatChanged)
			{
				QStringList Used;
				QStringList Rejected;
				for (int i = 0; i < ui.treeCompat->topLevelItemCount(); i++) {
					QTreeWidgetItem* pItem = ui.treeCompat->topLevelItem(i);
					if (pItem->checkState(0) == Qt::Unchecked)
						Rejected.append(pItem->data(0, Qt::UserRole).toString());
					else
						Used.append(pItem->data(0, Qt::UserRole).toString());
				}

				// retain local templates
				foreach(const QString& Template, theAPI->GetGlobalSettings()->GetTextList("Template", false)) {
					if (Template.left(6) == "Local_") {
						Used.append(Template);
					}
				}

				WriteTextList("Template", Used);
				WriteTextList("TemplateReject", Rejected);
				m_CompatChanged = false;
			}
		}
		catch (SB_STATUS Status)
		{
			theGUI->CheckResults(QList<SB_STATUS>() << Status);
		}
	}

	if (ui.chkAutoRoot->isVisible())
		theConf->SetValue("Options/PortableRootDir", CSettingsWindow__Chk2Int(ui.chkAutoRoot->checkState()));

	theConf->SetValue("Options/AutoRunSoftCompat", !ui.chkNoCompat->isChecked());

	if (m_CertChanged && theAPI->IsConnected())
	{
		QByteArray Certificate = ui.txtCertificate->toPlainText().toUtf8();	
		if (g_Certificate != Certificate) {

			QPalette palette = QApplication::palette();

			if (theGUI->m_DarkTheme)
				palette.setColor(QPalette::Text, Qt::black);

			ui.lblCertExp->setVisible(false);

			bool bRet = ApplyCertificate(Certificate, this);

			if (Certificate.isEmpty())
				palette.setColor(QPalette::Base, Qt::white);
			else if (!bRet) 
				palette.setColor(QPalette::Base, QColor(255, 192, 192));
			else if (g_CertInfo.expired || g_CertInfo.outdated) {
				palette.setColor(QPalette::Base, QColor(255, 255, 192));
				ui.lblCertExp->setVisible(true);
			}
			else
				palette.setColor(QPalette::Base, QColor(192, 255, 192));

			ui.txtCertificate->setPalette(palette);
		}

		m_CertChanged = false;
	}

	theConf->SetValue("Options/CheckForUpdates", CSettingsWindow__Chk2Int(ui.chkAutoUpdate->checkState()));
	theConf->SetValue("Options/DownloadUpdates", CSettingsWindow__Chk2Int(ui.chkAutoDownload->checkState()));
	//theConf->SetValue("Options/InstallUpdates", CSettingsWindow__Chk2Int(ui.chkAutoInstall->checkState()));

	theConf->SetValue("Options/NoSupportCheck", ui.chkNoCheck->isChecked());

	emit OptionsChanged(m_bRebuildUI);
}

bool CSettingsWindow::ApplyCertificate(const QByteArray &Certificate, QWidget* widget)
{
	QString CertPath = theAPI->GetSbiePath() + "\\Certificate.dat";
	if (!Certificate.isEmpty()) {

		auto Args = GetArguments(Certificate, L'\n', L':');

		bool bLooksOk = true;
		if (Args.value("NAME").isEmpty()) // mandatory
			bLooksOk = false;
		//if (Args.value("UPDATEKEY").isEmpty())
		//	bLooksOk = false;
		if (Args.value("SIGNATURE").isEmpty()) // absolutely mandatory
			bLooksOk = false;

		if (bLooksOk) {
			QString TempPath = QDir::tempPath() + "/Sbie+Certificate.dat";
			QFile CertFile(TempPath);
			if (CertFile.open(QFile::WriteOnly)) {
				CertFile.write(Certificate);
				CertFile.close();
			}

			WindowsMoveFile(TempPath.replace("/", "\\"), CertPath.replace("/", "\\"));
		}
		else {
			QMessageBox::critical(widget, "Sandboxie-Plus", tr("This does not look like a certificate. Please enter the entire certificate, not just a portion of it."));
			return false;
		}
	}
	else if(!g_Certificate.isEmpty()){
		WindowsMoveFile(CertPath.replace("/", "\\"), "");
	}

	if (Certificate.isEmpty())
		return false;

	if (!theAPI->ReloadCert().IsError())
	{
		g_FeatureFlags = theAPI->GetFeatureFlags();
		theGUI->UpdateCertState();

		if (g_CertInfo.expired || g_CertInfo.outdated) {
			if(g_CertInfo.expired)
				QMessageBox::information(widget, "Sandboxie-Plus", tr("This certificate is unfortunately expired."));
			else
				QMessageBox::information(widget, "Sandboxie-Plus", tr("This certificate is unfortunately outdated."));
		}
		else {
			QMessageBox::information(widget, "Sandboxie-Plus", tr("Thank you for supporting the development of Sandboxie-Plus."));
		}

		g_Certificate = Certificate;
		return true;
	}
	else
	{
		QMessageBox::critical(widget, "Sandboxie-Plus", tr("This support certificate is not valid."));

		g_CertInfo.State = 0;
		g_Certificate.clear();
		return false;
	}
}

void CSettingsWindow::apply()
{
	if (!ui.btnEditIni->isEnabled())
		SaveIniSection();
	else
		SaveSettings();
	LoadSettings();
}

void CSettingsWindow::ok()
{
	apply();

	this->close();
}

void CSettingsWindow::reject()
{
	this->close();
}

void CSettingsWindow::OnBrowse()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	ui.fileRoot->setText(Value + "\\%SANDBOX%");
}

void CSettingsWindow::OnChange()
{
	QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui.cmbOnClose->model());
	QStandardItem *item = model->item(0);
	item->setFlags((ui.cmbSysTray->currentIndex() == 0) ? item->flags() & ~Qt::ItemIsEnabled : item->flags() | Qt::ItemIsEnabled);

	if (ui.chkAutoRoot->isVisible())
		ui.fileRoot->setEnabled(ui.chkAutoRoot->checkState() != Qt::Checked);

	ui.btnSetPassword->setEnabled(ui.chkPassRequired->isChecked());
}

void CSettingsWindow::OnTab()
{
	if (ui.tabs->currentWidget() == ui.tabEdit)
	{
		LoadIniSection();
		ui.txtIniSection->setReadOnly(true);
	}
	else if (ui.tabs->currentWidget() == ui.tabCompat && m_CompatLoaded != 1 && theAPI->IsConnected())
	{
		if(m_CompatLoaded == 0)
			theGUI->GetCompat()->RunCheck();

		ui.treeCompat->clear();

		QMap<QString, int> Templates = theGUI->GetCompat()->GetTemplates();
		for (QMap<QString, int>::iterator I = Templates.begin(); I != Templates.end(); ++I)
		{
			if (I.value() == CSbieTemplates::eNone)
				continue;

			QSharedPointer<CSbieIni> pTemplate = QSharedPointer<CSbieIni>(new CSbieIni("Template_" + I.key(), theAPI));

			QString Title = pTemplate->GetText("Tmpl.Title", "", false, true, true);
			if (Title.left(1) == "#")
			{
				int End = Title.mid(1).indexOf(",");
				if (End == -1) End = Title.length() - 1;
				int MsgNum = Title.mid(1, End).toInt();
				Title = theAPI->GetSbieMsgStr(MsgNum, theGUI->m_LanguageId).arg(Title.mid(End + 2)).arg("");
			}
			//if (Title.isEmpty()) Title = Name;

			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setText(0, Title);
			pItem->setData(0, Qt::UserRole, I.key());
			if((I.value() & CSbieTemplates::eDisabled) != 0)
				pItem->setCheckState(0, Qt::Unchecked);
			else if((I.value() & CSbieTemplates::eEnabled) != 0)
				pItem->setCheckState(0, Qt::Checked);
			else
				pItem->setCheckState(0, Qt::PartiallyChecked);
			ui.treeCompat->addTopLevelItem(pItem);
		}

		m_CompatLoaded = 1;
		m_CompatChanged = true;
	}
}

void CSettingsWindow::OnSetPassword()
{
retry:
	QString Value1 = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter the new configuration password."), QLineEdit::Password);
	if (Value1.isEmpty())
		return;

	QString Value2 = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please re-enter the new configuration password."), QLineEdit::Password);
	if (Value2.isEmpty())
		return;

	if (Value1 != Value2) {
		QMessageBox::warning(this, "Sandboxie-Plus", tr("Passwords did not match, please retry."));
		goto retry;
	}

	m_NewPassword = Value1;
}

void CSettingsWindow::AddWarnEntry(const QString& Name, int type)
{
	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setText(0, (type == 1 ? tr("Process") : tr("Folder")));
	pItem->setData(0, Qt::UserRole, type);

	pItem->setData(1, Qt::UserRole, Name);
	pItem->setText(1, Name);
	ui.treeWarnProgs->addTopLevelItem(pItem);
}

void CSettingsWindow::OnAddWarnProg()
{
	QString Value = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter a program file name"));
	if (Value.isEmpty())
		return;
	AddWarnEntry(Value, 1);
	m_WarnProgsChanged = true;
}

void CSettingsWindow::OnAddWarnFolder()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;
	AddWarnEntry(Value, 2);
	m_WarnProgsChanged = true;
}

void CSettingsWindow::OnDelWarnProg()
{
	QTreeWidgetItem* pItem = ui.treeWarnProgs->currentItem();
	if (!pItem)
		return;

	delete pItem;
	m_WarnProgsChanged = true;
}

void CSettingsWindow::OnTemplateClicked(QTreeWidgetItem* pItem, int Column)
{
	// todo: check if really changed
	m_CompatChanged = true;
}

void CSettingsWindow::OnTemplateDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
	QSharedPointer<CSbieIni> pTemplate = QSharedPointer<CSbieIni>(new CSbieIni("Template_" + pItem->data(0, Qt::UserRole).toString(), theAPI));

	COptionsWindow OptionsWindow(pTemplate, pItem->text(1));
	OptionsWindow.exec();
}

void CSettingsWindow::OnAddCompat()
{
	QTreeWidgetItem* pItem = ui.treeCompat->currentItem();
	if (!pItem)
		return;

	pItem->setCheckState(0, Qt::Checked);
	m_CompatChanged = true;
}

void CSettingsWindow::OnDelCompat()
{
	QTreeWidgetItem* pItem = ui.treeCompat->currentItem();
	if (!pItem)
		return;

	pItem->setCheckState(0, Qt::Unchecked);
	m_CompatChanged = true;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Raw section ini Editor
//

void CSettingsWindow::SetIniEdit(bool bEnable)
{
	for (int i = 0; i < ui.tabs->count() - 2; i++) {
		bool Enabled = ui.tabs->widget(i)->isEnabled();
		ui.tabs->setTabEnabled(i, !bEnable && Enabled);
		ui.tabs->widget(i)->setEnabled(Enabled);
	}
	ui.btnSaveIni->setEnabled(bEnable);
	ui.btnCancelEdit->setEnabled(bEnable);
	ui.txtIniSection->setReadOnly(!bEnable);
	ui.btnEditIni->setEnabled(!bEnable);
}

void CSettingsWindow::OnEditIni()
{
	SetIniEdit(true);
}

void CSettingsWindow::OnSaveIni()
{
	SaveIniSection();
	SetIniEdit(false);
	LoadSettings();
}

void CSettingsWindow::OnCancelEdit()
{
	SetIniEdit(false);
}

void CSettingsWindow::LoadIniSection()
{
	QString Section;

	if(theAPI->IsConnected())
		Section = theAPI->SbieIniGetEx("GlobalSettings", "");

	ui.txtIniSection->setPlainText(Section);
}

void CSettingsWindow::SaveIniSection()
{
	if(theAPI->IsConnected())
		theAPI->SbieIniSet("GlobalSettings", "", ui.txtIniSection->toPlainText());

	LoadIniSection();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support
//

void CSettingsWindow::CertChanged() 
{ 
	m_CertChanged = true; 
	QPalette palette = QApplication::palette();
	ui.txtCertificate->setPalette(palette);
}

void CSettingsWindow::LoadCertificate(QString CertPath)
{
	if (theAPI && theAPI->IsConnected())
		CertPath = theAPI->GetSbiePath() + "\\Certificate.dat";
		
	QFile CertFile(CertPath);
	if (CertFile.open(QFile::ReadOnly)) {
		g_Certificate = CertFile.readAll();
		CertFile.close();
	}
}

#include <windows.h>
#include <shellapi.h>

void WindowsMoveFile(const QString& From, const QString& To)
{
	wstring from = From.toStdWString();
	from.append(L"\0", 1);
	wstring to = To.toStdWString();
	to.append(L"\0", 1);

	SHFILEOPSTRUCT SHFileOp;
    memset(&SHFileOp, 0, sizeof(SHFILEOPSTRUCT));
    SHFileOp.hwnd = NULL;
    SHFileOp.wFunc = To.isEmpty() ? FO_DELETE : FO_MOVE;
	SHFileOp.pFrom = from.c_str();
    SHFileOp.pTo = to.c_str();
    SHFileOp.fFlags = NULL;    

    //The Copying Function
    SHFileOperation(&SHFileOp);
}
