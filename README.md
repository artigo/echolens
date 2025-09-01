# ECHOLENS


## Dependencies

```
sudo apt install cmake make libpipewire-0.3-dev ffmpeg
```
```
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
```
```
chmod +x linuxdeploy-x86_64.AppImage
```
```
mkdir -p ~/.local/bin
```
```
mv linuxdeploy-x86_64.AppImage ~/.local/bin/linuxdeploy
```
```
echo 'PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
```
```
source ~/.bashrc
```

## Recorder Build
```
cd recorder
mkdir build
cmake ..
make
make AppImage
```

## Transcriber Build
```
cd transcriber
virtualenv venv
source venv/bin/active
pip install -r requirements.txt
pyinstaller --name transcriber_app --onefile --clean transcriber.py
```