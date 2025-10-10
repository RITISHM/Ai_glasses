from gtts import gTTS

tts = gTTS(
    text="high mera name google hai. me ek assistant hu jo aapki madad karega ",
    lang='en',
    tld='co.in',     # This gives you Indian English accent
    slow=False
)
tts.save("indian_accent.mp3")