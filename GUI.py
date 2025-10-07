import tkinter as tk
from tkinter import messagebox

def close_window():
    """Closes the Tkinter window after a confirmation dialog.

    This function displays a confirmation dialog to the user. If the user
    confirms, the main Tkinter window (`root`) is destroyed, terminating the
    application's GUI.
    """
    if messagebox.askokcancel("Quit", "Do you want to close the application?"):
        root.destroy()

# Create the main window
root = tk.Tk()
root.title("OLLAMA Running")
root.geometry("400x250")  # Width x Height
root.configure(bg="#f0f0f0")  # Light background color

# Adding a frame for better layout organization
frame = tk.Frame(root, bg="#f0f0f0")
frame.pack(pady=20)

# Label to indicate the main task is running
label = tk.Label(frame, text="Main Task is running...", font=("Arial", 16), bg="#f0f0f0")
label.pack(pady=20)

# Status indicator to show the application is active
status_label = tk.Label(frame, text="Status: Active", font=("Arial", 12), fg="green", bg="#f0f0f0")
status_label.pack(pady=5)

# Close button to exit the application
close_button = tk.Button(frame, text="Close", command=close_window, font=("Arial", 12), bg="#ff6666", fg="white", padx=10, pady=5)
close_button.pack(pady=10)

# Footer label with company information
footer = tk.Label(root, text="INFOLITZ SOFTWARE PRIVATE LIMITED", font=("Arial", 10), bg="#f0f0f0")
footer.pack(side=tk.BOTTOM, pady=10)

# Start the Tkinter event loop
root.mainloop()
