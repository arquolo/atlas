<<<<<<< Updated upstream:WorklistInterface/Worklist Interface/GUI/CompositeChild.h
#ifndef __ASAP_GUI_COMPOSITECHILD__
#define __ASAP_GUI_COMPOSITECHILD__
#include <QMainWindow>

namespace ASAP
{
	/// <summary>
	/// Provides an interface that interacts with the CompositeWindow class,
	/// allowing the implementation to request a tab or context switch 
	/// through a signal.
	/// </summary>
	class CompositeChild : public QMainWindow
	{
		Q_OBJECT
		public:
			CompositeChild(QWidget *parent);

		signals:
			void RequiresTabSwitch(int tab_id);
	};
}
=======
#ifndef __ASAP_GUI_COMPOSITECHILD__
#define __ASAP_GUI_COMPOSITECHILD__
#include <QMainWindow>

namespace ASAP::GUI
{
	/// <summary>
	/// Provides an interface that interacts with the CompositeWindow class,
	/// allowing the implementation to request a tab or context switch 
	/// through a signal.
	/// </summary>
	class CompositeChild : public QMainWindow
	{
		Q_OBJECT
		public:
			CompositeChild(QWidget *parent);

		signals:
			void RequiresTabSwitch(int tab_id);
	};
}
>>>>>>> Stashed changes:_legacy/worklist_interface/gui/composite_child.h
#endif // __ASAP_GUI_COMPOSITECHILD__