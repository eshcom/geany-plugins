const gchar *message1 = "text";
const gchar *message2 = "text";
ui_set_statusbar(TRUE, "message1 = %s", message1); // esh: log
ui_set_statusbar(TRUE, "message1 = %s, message2 = %s", message1, message2); // esh: log

ui_set_statusbar(TRUE, "No more message items."); // esh: log
----------------------------------------------------------------------------------------

