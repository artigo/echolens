# ECHOLENS


## Dependencies

`sudo apt install cmake make libpipewire-0.3-dev`

`wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage`

`chmod +x linuxdeploy-x86_64.AppImage`

`mkdir -p ~/.local/bin`

`mv linuxdeploy-x86_64.AppImage ~/.local/bin/linuxdeploy`

`echo '' >> ~/.profile`
`echo '# add ~/.local/bin to PATH if it exists' >> ~/.profile`
`echo 'if [ -d "$HOME/.local/bin" ] ; then' >> ~/.profile`
`echo '    PATH="$HOME/.local/bin:$PATH"' >> ~/.profile`
`echo 'fi' >> ~/.profile`
`source ~/.profile`

## Build
`mkdir build`
`cmake ..`
`make`
`make AppImage`