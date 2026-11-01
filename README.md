# ZeroEd - A High-Performance Native C++ AI Lecture Engine

**ZeroEd** is a lightweight, blazing-fast command-line utility built entirely in native C++17. It addresses student information overload and academic fatigue by instantly processing long lecture notes into structured summaries and active self-assessment flashcards with zero runtime lag and minimal RAM usage.

Unlike heavy, web-wrapped AI productivity tools, ZeroEd interfaces directly with the **Google Gemini 1.5-Flash API** using custom-escaped JSON strings over native **libcurl** sockets.

---

## 🚀 Features

- **Zero Framework Bloat:** Ultra-lightweight terminal interface optimized for raw execution speed.
- **Direct AI Streaming:** Fast data connections stream structured active-recall cards right to your console window.
- **Smart Summarization:** Automatically parses raw classroom text notes.
- **Local Markdown Export:** Saves generated study metrics and flashcards into permanent local `.md` notes.

---

## 🛠️ Prerequisites & Tech Stack

- **Language:** Native C++17
- **Compiler:** `g++` (GCC) 
- **Dependencies:** `libcurl` development library
- **API Key:** Google Gemini API Key

---

## 📥 Setup and Installation

### 1. Install Dependencies

Before compiling, make sure your system has a C++ compiler and the `libcurl` library installed.

- **On Ubuntu/Linux:**
  ```bash
  sudo apt-get update
  sudo apt-get install build-essential libcurl4-openssl-dev
  ```

- **On macOS:**
  ```bash
  brew install curl
  ```

### 2. Set Up Your Environment Variable
Set your Gemini API key in your terminal session so the application can authenticate safely:

- **On Linux/macOS:**
  ```bash
  export GEMINI_API_KEY="your_actual_api_key_here"
  ```

---

## 💻 Compilation & Usage

### 1. Prepare Your Input File
Place your lecture notes inside a text file named `lecture.txt` in the root folder of this project:
```bash
echo "Paste your long lecture notes or textbook chapters here" > lecture.txt
```

### 2. Compile the Program
Compile the code using `g++` while linking the native `libcurl` sockets:
```bash
g++ -std=c++17 main.cpp $(curl-config --cflags --libs) -o lecture_summary
```

### 3. Run the Application
Execute the compiled binary:
```bash
./lecture_summary
```

---

## 📂 Project Structure

```text
├── main.cpp          # Core C++ network pipeline & Gemini API interface
├── lecture.txt       # Raw input student lecture notes (User provided)
└── README.md         # Project documentation and setup guide
```

---

## 👤 Developer
- **Rudra Sachin Deshmukh** - Computer Science & Engineering (1st Semester), Indian Institute of Information Technology Pune.
