# import server
# server.run()
import google.generativeai as genai
from dotenv import load_dotenv
import os

load_dotenv()  # loads from .env in root

SECRET_KEY = os.getenv("GEMINI_API_KEY")
MODEL_ID=os.getenv("MODEL_ID")

# Set your API key
genai.configure(api_key=SECRET_KEY)

# Choose a Gemini model (e.g., Gemini 1.5 Flash or Pro)
model = genai.GenerativeModel(MODEL_ID)

# Make a simple API call
response = model.generate_content("Write a short poem about the ocean.")

# Print the response
print(response.text)
