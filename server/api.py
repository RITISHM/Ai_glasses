import google.generativeai as genai
from dotenv import load_dotenv
import os
from PIL import Image 
import json
from datetime import datetime

load_dotenv()  # loads from .env in root
chat=None
SECRET_KEY = os.getenv("GEMINI_API_KEY")
MODEL_ID=os.getenv("MODEL_ID")
INSTRUCTION=os.getenv("INSTRUCTIONS")
TIMESTAMP=""
# Set your API key
genai.configure(api_key=SECRET_KEY)
llm_model = genai.GenerativeModel(MODEL_ID, system_instruction=INSTRUCTION)
print ("API setup is done ✅")

def start_chat():
  chat=llm_model.start_chat()
  TIMESTAMP=datetime.now().strftime("%Y-%m-%d-%H:%M:%S")
  print("New chat started")

def generate_image_response(image_loc,prompt):
  image = Image.open(image_loc)
  response=llm_model.model.generate_content([prompt,image])
  return response

def generate_prompt_response(prompt):
  response=llm_model.model.generate_content(prompt)
  return response

def end_chat(loc):
  loc=os.path.join(loc,TIMESTAMP)
  with open(loc, "w") as f:
    json.dump(chat.history, f)
  print("chat ended and saved in:",loc)
  chat=None