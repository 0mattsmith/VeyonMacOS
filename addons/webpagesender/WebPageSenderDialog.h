/*
 * WebPageSenderDialog.h - declaration of WebPageSenderDialog class
 *
 * Copyright (c) 2026 Veyon Community
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <QDialog>

#include "WebPageSenderPlugin.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class VeyonMasterInterface;

class WebPageSenderDialog : public QDialog
{
	Q_OBJECT
public:
	WebPageSenderDialog( VeyonMasterInterface& master, int computerCount, QWidget* parent = nullptr );
	~WebPageSenderDialog() override = default;

	const WebPageSenderPlugin::WebPage& webPage() const
	{
		return m_webPage;
	}

	void accept() override;

private:
	static constexpr auto MaximumHistorySize = 25;

	void updateOptions();

	VeyonMasterInterface& m_master;
	WebPageSenderPlugin::WebPage m_webPage;

	QComboBox* m_urlComboBox{nullptr};
	QLineEdit* m_titleEdit{nullptr};
	QRadioButton* m_openButton{nullptr};
	QRadioButton* m_notifyButton{nullptr};
	QRadioButton* m_shortcutButton{nullptr};
	QComboBox* m_browserComboBox{nullptr};
	QCheckBox* m_newWindowCheckBox{nullptr};
	QCheckBox* m_fullScreenCheckBox{nullptr};

};
