import argparse
import whisper


def transcribe(model_name, wav_file):
    model = whisper.load_model(model_name)
    result = model.transcribe(wav_file)
    return result



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("audio_file", type=str, help="Path to the audio file (mp3, wav, ...).")
    parser.add_argument("--model_name", type=str, default="turbo", help="Whisper model name (turbo, base, ...).")
    parser.add_argument("--output_file", type=str, default="transcript.txt", help="Outputted transcript file path.")
    args = parser.parse_args()
    text = transcribe(args.model_name, args.audio_file)['text']
    with open(args.output_file, "w") as f:
        f.write(text)
        
    

