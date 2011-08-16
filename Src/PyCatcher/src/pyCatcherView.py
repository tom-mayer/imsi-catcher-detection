import sys
import pygtk
import gtk #@UnresolvedImport
import gtk.glade #@UnresolvedImport

class PyCatcherGUI:
    
    def __init__(self, catcher_controller):
        self.scan_toggled_on = False
        self.catcher_controller = catcher_controller
        
        self.w_tree = gtk.glade.XML("../GUI/mainWindow.glade")        
        self.main_window = self.w_tree.get_widget("main_window")
        signals = {"on_main_window_destroy": gtk.main_quit,
                   "on_scan_toggle_toggled": self._toggle_scan
                   }
        self.w_tree.signal_autoconnect(signals)
        
        self.bs_view = self.w_tree.get_widget("bs_table")
        self._add_column("foo", 0)
        self._add_column("bar", 1)
        
        
    def _add_column(self, name, index):
        column = gtk.TreeViewColumn(name, gtk.CellRendererText(), text=index)
        column.set_resizable(True)
        column.set_sort_column_id(index)
        self.bs_view.append_column(column)
        
    def _toggle_scan (self, widget):
        if(not self.scan_toggled_on):
            print "toggle on"
            self.catcher_controller.start_scan()
            self.scan_toggled_on = True
        else:
            print "toggle off"
            self.catcher_controller.stop_scan()
            self.scan_toggled_on = False
    