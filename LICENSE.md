## Audionaut license

Audionaut is published under a dual [GPL3 (or later)](https://www.gnu.org/licenses/gpl-3.0.en.html)/Commercial license.

If you want to use Audionaut in a closed-source project, you need to purchase a commercial license. Please contact us for more information [vltmrkls@gmail.com](mailto:vltmrkls@gmail.com?subject=License%20Request).

**Note:** commercial licenses are not yet available. Audionaut currently links against GPL/AGPL-licensed third-party libraries (Essentia, Ableton Link) for which no commercial/relicensing agreement is in place, so a closed-source license cannot be issued at this time.

Pitch-preserving time-stretch (the Stretch clip mode) is built on [Signalsmith Stretch](https://signalsmith-audio.co.uk/code/stretch/) (MIT) and its companion library [signalsmith-linear](https://github.com/Signalsmith-Audio/linear) (MIT).

Development builds can also carry three alternative stretch engines for evaluation, selectable in Settings ▸ Stretch: [Bungee](https://github.com/bungee-audio-stretch/bungee) (MPL-2.0, with its bundled Eigen (MPL-2.0) and PFFFT (BSD-like)), [Rubber Band Library](https://breakfastquay.com/rubberband/) (GPL-2.0 or later; a commercial licence is available from the authors and would be required for a closed-source build) and [SoundTouch](https://www.surina.net/soundtouch/) (LGPL-2.1). Each is enabled by a build flag (`STRETCH_BUNGEE_ENABLED`, `STRETCH_RUBBERBAND_ENABLED`, `STRETCH_SOUNDTOUCH_ENABLED`); release builds for the Mac App Store must leave Rubber Band out.

Stem separation is built on [demucs.cpp](https://github.com/sevagh/demucs.cpp) (MIT). The Demucs *htdemucs* model weights it runs are not part of this source distribution or the installers; the application downloads them separately on first use. Note that Meta has stated the pretrained weights are not covered by the MIT licence and are provided for scientific purposes only ([facebookresearch/demucs#327](https://github.com/facebookresearch/demucs/issues/327)).
