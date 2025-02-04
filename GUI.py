import tkinter as tk
from tkinter import messagebox

def close_window():
    if messagebox.askokcancel("Quit", "Do you want to close the application?"):
        root.destroy()

root = tk.Tk()
root.title("OLLAMA Running")
root.geometry("400x250")  # Width x Height
root.configure(bg="#f0f0f0")  # Light background color

# Adding a frame for better layout
frame = tk.Frame(root, bg="#f0f0f0")
frame.pack(pady=20)

label = tk.Label(frame, text="Main Task is running...", font=("Arial", 16), bg="#f0f0f0")
label.pack(pady=20)

# Adding a status indicator
status_label = tk.Label(frame, text="Status: Active", font=("Arial", 12), fg="green", bg="#f0f0f0")
status_label.pack(pady=5)

# Close button with a better look
close_button = tk.Button(frame, text="Close", command=close_window, font=("Arial", 12), bg="#ff6666", fg="white", padx=10, pady=5)
close_button.pack(pady=10)

# Adding a footer
footer = tk.Label(root, text="INFOLITZ SOFTWARE PRIVATE LIMITED", font=("Arial", 10), bg="#f0f0f0")
footer.pack(side=tk.BOTTOM, pady=10)

root.mainloop()
