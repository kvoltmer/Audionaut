## Audionaut license

Audionaut is published under a dual [GPL3 (or later)](https://www.gnu.org/licenses/gpl-3.0.en.html)/Commercial license.

If you want to use Audionaut in a closed-source project, you need to purchase a commercial license. Please contact us for more information [vltmrkls@gmail.com](mailto:vltmrkls@gmail.com?subject=License%20Request).

**Note:** commercial licenses are not yet available. Audionaut currently links against GPL/AGPL-licensed third-party libraries (Essentia, Ableton Link) for which no commercial/relicensing agreement is in place, so a closed-source license cannot be issued at this time.

Pitch-preserving time-stretch (the Stretch clip mode) is built on [Signalsmith Stretch](https://signalsmith-audio.co.uk/code/stretch/) (MIT) and its companion library [signalsmith-linear](https://github.com/Signalsmith-Audio/linear) (MIT).

Stem separation is built on [demucs.cpp](https://github.com/sevagh/demucs.cpp) (MIT). The Demucs *htdemucs* model weights it runs are not part of this source distribution or the installers; the application downloads them separately on first use. Note that Meta has stated the pretrained weights are not covered by the MIT licence and are provided for scientific purposes only ([facebookresearch/demucs#327](https://github.com/facebookresearch/demucs/issues/327)).
