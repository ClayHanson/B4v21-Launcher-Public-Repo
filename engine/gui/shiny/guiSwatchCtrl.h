#ifndef _GUISWATCHCTRL_H_
#define _GUISWATCHCTRL_H_

#ifndef _GUICONTROL_H_
#include "gui/core/guiControl.h"
#endif

#include "dgl/dgl.h"
#include "console/console.h"
#include "console/consoleTypes.h"

class GuiSwatchCtrl : public GuiControl {
	typedef GuiControl Parent;

private:
	ColorI mFillColor;
	ColorI mBorderColor;
	bool mFilled;
	bool mBorder;

public:
	GuiSwatchCtrl();
	void onRender(Point2I offset, const RectI &updateRect);

	static void initPersistFields();
	DECLARE_CONOBJECT(GuiSwatchCtrl);
};

#endif