import locale
import gtk
from cellIDDatabase import CIDDatabases
from xdot import DotWidget
import datetime
import time
from rules import RuleResult

class PyCatcherGUI:
 
    def __init__(self, catcher_controller):
        encoding = locale.getlocale()[1]
        self._utf8conv = lambda x : unicode(x, encoding).encode('utf8')
        
        self._builder = gtk.Builder()
        self._builder.add_from_file('../GUI/catcher_main.glade')
        self._main_window = self._builder.get_object('main_window')
        self._main_window.show()
        
        self._filter_window = self._builder.get_object('filter_window')
        self._detail_window = self._builder.get_object('detail_view')
        self._rules_window = self._builder.get_object('rules_window')
        self._evaluators_window = self._builder.get_object('evaluators_window')
        self._evaluation_window = self._builder.get_object('evaluation_window')
        self._evaluation_image = self._builder.get_object('evaluation_image')
        self._databases_window = self._builder.get_object('databases_window')

        self._ok_image = gtk.gdk.pixbuf_new_from_file('../GUI/Images/ok.png')
        self._warning_image = gtk.gdk.pixbuf_new_from_file('../GUI/Images/warning.png')
        self._critical_image = gtk.gdk.pixbuf_new_from_file('../GUI/Images/critical.png')
        self._plain_image = gtk.gdk.pixbuf_new_from_file('../GUI/Images/plain.png')
        self.set_image(RuleResult.IGNORE)

        self._catcher_controller = catcher_controller        
        
        self._bs_tree_view = self._builder.get_object('tv_stations')
        self._add_column("Provider", 0)
        self._add_column("ARFCN", 1)
        self._add_column("Strength",2)
        self._add_column("Cell ID",3)
        self._add_column("Evaluation",4)
        self._add_column("Last seen", 5)
        self._add_column('# Scanned',6)
        self._bs_tree_view.set_model(self._catcher_controller.bs_tree_list_data)       
            
        self._horizontal_container = self._builder.get_object('vbox2')
        self._dot_widget = DotWidget()
        self._horizontal_container.pack_start_defaults(self._dot_widget)
        self._dot_widget.set_filter('neato')
        self._dot_widget.show()
        self._dot_widget.connect('clicked', self._on_graph_node_clicked)

        self._builder.connect_signals(self)
        
        detail_view_text = self._builder.get_object('te_detail_view')
        self._detail_buffer = detail_view_text.get_buffer()        
        
        log_view = self._builder.get_object('te_log')
        self._log_buffer = log_view.get_buffer()        
        self._log_buffer.insert(self._log_buffer.get_end_iter(),self._utf8conv("-- Log execution on " + datetime.datetime.now().strftime("%A, %d. %B %Y %I:%M %p") + "  --\n\n"))
        
        self._main_window.show()

    def set_image(self, status):
        pixbuf = self._plain_image
        if status == RuleResult.OK:
            pixbuf = self._ok_image
        elif status == RuleResult.WARNING:
            pixbuf = self._warning_image
        elif status == RuleResult.CRITICAL:
            pixbuf = self._critical_image
        width, height = self._evaluation_window.get_size()
        pixbuf = pixbuf.scale_simple(width, height, gtk.gdk.INTERP_BILINEAR)
        self._evaluation_image.set_from_pixbuf(pixbuf)
        
    def _add_column(self, name, index):
        column = gtk.TreeViewColumn(name, gtk.CellRendererText(), text=index)
        column.set_resizable(True)
        column.set_sort_column_id(index)
        self._bs_tree_view.append_column(column)
    
    def _update_filters(self):
        if self._builder.get_object('cb_filter_by_provider').get_active():
            self._catcher_controller.provider_filter.params = {'providers': self._builder.get_object('te_filter_provider').get_text()}
            self._catcher_controller.provider_filter.is_active = True
        else: 
            self._catcher_controller.provider_filter.is_active = False
                                                               
        if self._builder.get_object('cb_filter_by_arfcn').get_active():
            self._catcher_controller.arfcn_filter.params = {'from':int(self._builder.get_object('te_filter_arfcn_from').get_text()),
                                         'to':int(self._builder.get_object('te_filter_arfcn_to').get_text())}
            self._catcher_controller.arfcn_filter.is_active = True
        else:
            self._catcher_controller.arfcn_filter.is_active = False

        self._catcher_controller.trigger_evaluation()

    def _update_rules(self):
        self._catcher_controller.provider_rule.is_active = self._builder.get_object('cb_provider_known').get_active()
        self._catcher_controller.country_mapping_rule.is_active = self._builder.get_object('cb_country_provider').get_active()
        self._catcher_controller.arfcn_mapping_rule.is_active = self._builder.get_object('cb_arfcn_provider').get_active()
        self._catcher_controller.lac_mapping_rule.is_active = self._builder.get_object('cb_lac_provider').get_active()
        self._catcher_controller.unique_cell_id_rule.is_active = self._builder.get_object('cb_uniqueness').get_active()
        self._catcher_controller.lac_median_rule.is_active = self._builder.get_object('cb_lac').get_active()
        self._catcher_controller.neighbourhood_structure_rule.is_active = self._builder.get_object('cb_neighbourhood_structure').get_active()
        self._catcher_controller.pure_neighbourhood_rule.is_active = self._builder.get_object('cb_pure_neighbourhood').get_active()
        self._catcher_controller.full_discovered_neighbourhoods_rule.is_active = self._builder.get_object('cb_neighbours_discovered').get_active()
        self._catcher_controller.cell_id_db_rule.is_active = self._builder.get_object('cb_cell_id_database').get_active()
        self._catcher_controller.location_area_database_rule.is_active = self._builder.get_object('cb_local_area_database').get_active()
        self._catcher_controller.trigger_evaluation()

    def _update_evaluators(self):
        pass

    def _on_csv_clicked(self, widget):
        self._update_databases()
        self._catcher_controller.export_csv()

    def _update_databases(self):
        self._catcher_controller.use_google =  self._builder.get_object('cb_google').get_active()
        self._catcher_controller.use_open_cell_id = self._builder.get_object('cb_opencellid').get_active()
        self._catcher_controller.use_local_db =  (self._builder.get_object('cb_cellid_database_local').get_active(),
                                                  self._builder.get_object('te_cellid_database').get_text())
        self._catcher_controller.set_new_location(self._builder.get_object('te_current_location').get_text())

    def show_info(self, message, title='PyCatcher', time_to_sleep=3, type='INFO'):
        gtk_type = {'INFO' : gtk.MESSAGE_INFO,
                    'ERROR': gtk.MESSAGE_ERROR}

        dlg = gtk.MessageDialog(type=gtk.MESSAGE_INFO,
                                message_format=str(message)
        )
        dlg.set_title(title)
        dlg.show()
        time.sleep(time_to_sleep)
        dlg.destroy()

    def log_line(self, line):
        self._log_buffer.insert(self._log_buffer.get_end_iter(),self._utf8conv(datetime.datetime.now().strftime("%I:%M:%S %p")+ ":     " + line + "\n"))
    
    def _on_graph_node_clicked (self, widget, url, event):
        print 'NODE CLICKED'

    def _on_web_services_clicked(self, widget):
        self._update_databases()
        self._catcher_controller.update_with_web_services()

    def _on_localdb_add_clicked(self, widget):
        self._update_databases()
        self._catcher_controller.update_location_database()

    def _on_main_window_destroy(self, widget):
        self._catcher_controller.shutdown() 
        gtk.main_quit()
        
    def _on_scan_toggled(self, widget):
        if widget.get_active():
            self._catcher_controller.start_scan()
        else:
            self._catcher_controller.stop_scan()
            
    def _on_firmware_toggled(self, widget):
        if widget.get_active():
            self._catcher_controller.start_firmware()
        else:
            self._catcher_controller.stop_firmware()

    def _on_filter_clicked(self,widget):
        self._filter_window.show()

    def _on_evaluation_clicked(self,widget):
        self._evaluation_window.show()

    def _on_filter_close_clicked(self, widget):
        self._update_filters()
        self._filter_window.hide()

    def _on_rules_close_clicked(self, widget):
        self._update_rules()
        self._rules_window.hide()
    
    def _on_evaluators_clicked(self, widget):
        self._evaluators_window.show()

    def _on_evaluators_window_close_clicked(self, window, event):
        window.hide()
        return True

    def _on_rules_clicked(self, widget):
        self._rules_window.show()
    
    def _on_evaluators_close_clicked(self, widget):
        self._update_evaluators()
        self._evaluators_window.hide()

    def _on_tv_stations_clicked(self, widget, unecessary_parameter_just_for_signature):
        selection = widget.get_selection()
        model, treeiter = selection.get_selected()
        if treeiter is not None:
            arfcn = model[treeiter][1]
            report = self._catcher_controller.fetch_report(arfcn)
            self._detail_buffer.set_text(self._utf8conv(report))
            self._detail_window.show()

    def _on_details_delete(self, widget, dunno_what_this_param_does):
        self._detail_window.hide()
        return True

    def _on_databases_clicked(self, widget):
        self._databases_window.show()

    def _on_databases_close_clicked(self, widget):
        self._update_databases()
        self._catcher_controller.trigger_evaluation()
        self._databases_window.hide()


    def _on_save_clicked(self, widget):
        chooser = gtk.FileChooserDialog(title="Save Project",
            action=gtk.FILE_CHOOSER_ACTION_SAVE,
            buttons=(gtk.STOCK_CANCEL,
                     gtk.RESPONSE_CANCEL,
                     gtk.STOCK_SAVE,
                     gtk.RESPONSE_OK))
        chooser.set_default_response(gtk.RESPONSE_OK)
        filter = gtk.FileFilter()
        filter.set_name("Catcher Project Files")
        filter.add_pattern("*.cpf")
        chooser.add_filter(filter)
        filter = gtk.FileFilter()
        filter.set_name("All files")
        filter.add_pattern("*")
        chooser.add_filter(filter)
        if chooser.run() == gtk.RESPONSE_OK:
            filename = chooser.get_filename()
            chooser.destroy()
            self._catcher_controller.save_project(filename)
        else:
            chooser.destroy()

    def _on_load_clicked(self, widget):
        chooser = gtk.FileChooserDialog(title="Open Project",
            action=gtk.FILE_CHOOSER_ACTION_OPEN,
            buttons=(gtk.STOCK_CANCEL,
                     gtk.RESPONSE_CANCEL,
                     gtk.STOCK_OPEN,
                     gtk.RESPONSE_OK))
        chooser.set_default_response(gtk.RESPONSE_OK)
        filter = gtk.FileFilter()
        filter.set_name("Catcher Project Files")
        filter.add_pattern("*.cpf")
        chooser.add_filter(filter)
        filter = gtk.FileFilter()
        filter.set_name("All files")
        filter.add_pattern("*")
        chooser.add_filter(filter)
        if chooser.run() == gtk.RESPONSE_OK:
            filename = chooser.get_filename()
            chooser.destroy()
            self._catcher_controller.load_project(filename)
        else:
            chooser.destroy()

    #---------------- Viewer Bindings ----------------------------------------------------#
    def _on_open_file_clicked(self, widget):
        chooser = gtk.FileChooserDialog(title="Open dot File",
                                        action=gtk.FILE_CHOOSER_ACTION_OPEN,
                                        buttons=(gtk.STOCK_CANCEL,
                                                 gtk.RESPONSE_CANCEL,
                                                 gtk.STOCK_OPEN,
                                                 gtk.RESPONSE_OK))
        chooser.set_default_response(gtk.RESPONSE_OK)
        filter = gtk.FileFilter()
        filter.set_name("Graphviz dot files")
        filter.add_pattern("*.dot")
        chooser.add_filter(filter)
        filter = gtk.FileFilter()
        filter.set_name("All files")
        filter.add_pattern("*")
        chooser.add_filter(filter)
        if chooser.run() == gtk.RESPONSE_OK:
            filename = chooser.get_filename()
            chooser.destroy()
            self.load_dot_from_file(filename)
        else:
            chooser.destroy()
    
    def _on_zoon_in_clicked(self,widget):
        self._dot_widget.on_zoom_in(None)
    
    def _on_zoon_out_clicked(self,widget):
        self._dot_widget.on_zoom_out(None)
    
    def _on_zoon_fit_clicked(self,widget):
        self._dot_widget.on_zoom_fit(None)
    
    def _on_zoon_original_clicked(self,widget):
        self._dot_widget.on_zoom_100(None)
    
    def load_dot_from_file(self, filename):
        try:
            fp = file(filename, 'rt')
            self.load_dot(fp.read(), filename)
            fp.close()
        except IOError, ex:
            self.show_info(ex)
    
    def load_dot(self, dotcode, filename="<stdin>"):
        if self._dot_widget.set_dotcode(dotcode, filename):
            #self._dot_widget.zoom_to_fit()
            pass
    
