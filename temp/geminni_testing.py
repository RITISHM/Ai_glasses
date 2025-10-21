import google.generativeai as genai

genai.configure(api_key="AIzaSyAQAVUUgsQ7vz78LVCN-IS8aDXYKstNSTk")
model = genai.GenerativeModel(model_name="gemini-pro")
response = model.generate_content("hi")
print(response)