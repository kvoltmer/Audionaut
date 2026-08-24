# Automatically generated makefile, created by the Projucer
# Don't edit this file! Your changes will be overwritten when you re-save the Projucer project!

# build with "V=1" for verbose builds
ifeq ($(V), 1)
V_AT =
else
V_AT = @
endif

# (this disables dependency generation if multiple architectures are set)
DEPFLAGS := $(if $(word 2, $(TARGET_ARCH)), , -MMD)

ifndef PKG_CONFIG
  PKG_CONFIG=pkg-config
endif

ifndef STRIP
  STRIP=strip
endif

ifndef AR
  AR=ar
endif

ifndef CONFIG
  CONFIG=Debug
endif

JUCE_ARCH_LABEL := $(shell uname -m)

ifeq ($(CONFIG),Debug)
  JUCE_BINDIR := build
  JUCE_LIBDIR := build
  JUCE_OBJDIR := build/intermediate/Debug
  JUCE_OUTDIR := build

  ifeq ($(TARGET_ARCH),)
    TARGET_ARCH := 
  endif

  JUCE_CPPFLAGS := $(DEPFLAGS) "-DLINUX=1" "-DDEBUG=1" "-D_DEBUG=1" "-DJUCE_PROJUCER_VERSION=0x8000c" "-DJUCE_MODULE_AVAILABLE_juce_analytics=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_basics=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_formats=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_processors=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1" "-DJUCE_MODULE_AVAILABLE_juce_core=1" "-DJUCE_MODULE_AVAILABLE_juce_data_structures=1" "-DJUCE_MODULE_AVAILABLE_juce_dsp=1" "-DJUCE_MODULE_AVAILABLE_juce_events=1" "-DJUCE_MODULE_AVAILABLE_juce_graphics=1" "-DJUCE_MODULE_AVAILABLE_juce_gui_basics=1" "-DJUCE_MODULE_AVAILABLE_juce_gui_extra=1" "-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1" "-DJUCE_STRICT_REFCOUNTEDPOINTER=1" "-DJUCE_STANDALONE_APPLICATION=1" "-DJUCE_MODAL_LOOPS_PERMITTED=1" "-DAUTO_EDIT_ENABLED=0" "-DGA4_API_SECRET=\"TpBEa6eRRAGeG4BZmqwJDA\"" "-DGA4_MEASUREMENT_ID=\"G-Q6904SZWWY\"" "-DLINK_PLATFORM_LINUX=1" "-DJUCER_LINUX_MAKE_6D53C8B4=1" "-DJUCE_APP_VERSION=1.3.0" "-DJUCE_APP_VERSION_HEX=0x10300" $(shell $(PKG_CONFIG) --cflags $(shell ($(PKG_CONFIG) --exists webkit2gtk-4.1 && echo webkit2gtk-4.1) || echo webkit2gtk-4.0) alsa freetype2 fontconfig libcurl gtk+-x11-3.0) -pthread -I../../JuceLibraryCode -I../../../Submodules/JUCE/modules -I../../Source -I../../../Submodules/essentia/src -I../../../Submodules/essentia/packaging/debian_3rdparty/include/eigen3 -I../../../Submodules/json/include/ -I../../../Submodules/link/include/ -I../../../Submodules/link/modules/asio-standalone/asio/include -I../../../Submodules/farbot/include/ $(CPPFLAGS)
  JUCE_CPPFLAGS_APP :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=0" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=0" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_TARGET_APP := audionaut

  JUCE_CFLAGS += $(JUCE_CPPFLAGS) $(TARGET_ARCH) -g -ggdb -O0 -Wno-multichar $(CFLAGS)
  JUCE_CXXFLAGS += $(JUCE_CFLAGS) -std=c++20 $(CXXFLAGS)
  JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) $(shell $(PKG_CONFIG) --libs alsa freetype2 fontconfig libcurl) -fvisibility=hidden ../../../Submodules/essentia/build/src/libessentia.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libchromaprint.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libavformat.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libavcodec.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libswresample.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libavutil.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libmp3lame.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libfftw3f.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libz.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libsamplerate.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libyaml.a -ldrm -lrt -ldl -lpthread $(LDFLAGS)

  CLEANCMD = rm -rf $(JUCE_OUTDIR)/$(JUCE_TARGET_APP) $(JUCE_OBJDIR)
endif

ifeq ($(CONFIG),Release)
  JUCE_BINDIR := build
  JUCE_LIBDIR := build
  JUCE_OBJDIR := build/intermediate/Release
  JUCE_OUTDIR := build

  ifeq ($(TARGET_ARCH),)
    TARGET_ARCH := 
  endif

  JUCE_CPPFLAGS := $(DEPFLAGS) "-DLINUX=1" "-DNDEBUG=1" "-DJUCE_PROJUCER_VERSION=0x8000c" "-DJUCE_MODULE_AVAILABLE_juce_analytics=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_basics=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_devices=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_formats=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_processors=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1" "-DJUCE_MODULE_AVAILABLE_juce_audio_utils=1" "-DJUCE_MODULE_AVAILABLE_juce_core=1" "-DJUCE_MODULE_AVAILABLE_juce_data_structures=1" "-DJUCE_MODULE_AVAILABLE_juce_dsp=1" "-DJUCE_MODULE_AVAILABLE_juce_events=1" "-DJUCE_MODULE_AVAILABLE_juce_graphics=1" "-DJUCE_MODULE_AVAILABLE_juce_gui_basics=1" "-DJUCE_MODULE_AVAILABLE_juce_gui_extra=1" "-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1" "-DJUCE_STRICT_REFCOUNTEDPOINTER=1" "-DJUCE_STANDALONE_APPLICATION=1" "-DJUCE_MODAL_LOOPS_PERMITTED=1" "-DAUTO_EDIT_ENABLED=0" "-DGA4_API_SECRET=\"TpBEa6eRRAGeG4BZmqwJDA\"" "-DGA4_MEASUREMENT_ID=\"G-Q6904SZWWY\"" "-DLINK_PLATFORM_LINUX=1" "-DJUCER_LINUX_MAKE_6D53C8B4=1" "-DJUCE_APP_VERSION=1.3.0" "-DJUCE_APP_VERSION_HEX=0x10300" $(shell $(PKG_CONFIG) --cflags $(shell ($(PKG_CONFIG) --exists webkit2gtk-4.1 && echo webkit2gtk-4.1) || echo webkit2gtk-4.0) alsa freetype2 fontconfig libcurl gtk+-x11-3.0) -pthread -I../../JuceLibraryCode -I../../../Submodules/JUCE/modules -I../../Source -I../../../Submodules/essentia/src -I../../../Submodules/essentia/packaging/debian_3rdparty/include/eigen3 -I../../../Submodules/json/include/ -I../../../Submodules/link/include/ -I../../../Submodules/link/modules/asio-standalone/asio/include -I../../../Submodules/farbot/include/ $(CPPFLAGS)
  JUCE_CPPFLAGS_APP :=  "-DJucePlugin_Build_VST=0" "-DJucePlugin_Build_VST3=0" "-DJucePlugin_Build_AU=0" "-DJucePlugin_Build_AUv3=0" "-DJucePlugin_Build_AAX=0" "-DJucePlugin_Build_Standalone=0" "-DJucePlugin_Build_Unity=0" "-DJucePlugin_Build_LV2=0"
  JUCE_TARGET_APP := audionaut

  JUCE_CFLAGS += $(JUCE_CPPFLAGS) $(TARGET_ARCH) -O3 -Wno-multichar $(CFLAGS)
  JUCE_CXXFLAGS += $(JUCE_CFLAGS) -std=c++20 $(CXXFLAGS)
  JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) $(shell $(PKG_CONFIG) --libs alsa freetype2 fontconfig libcurl) -fvisibility=hidden ../../../Submodules/essentia/build/src/libessentia.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libchromaprint.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libavformat.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libavcodec.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libswresample.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libavutil.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libmp3lame.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libfftw3f.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libz.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libsamplerate.a ../../../Submodules/essentia/packaging/debian_3rdparty/lib/libyaml.a -ldrm -lrt -ldl -lpthread $(LDFLAGS)

  CLEANCMD = rm -rf $(JUCE_OUTDIR)/$(JUCE_TARGET_APP) $(JUCE_OBJDIR)
endif

OBJECTS_APP := \
  $(JUCE_OBJDIR)/AudiumMenuModel_e3ecba7b.o \
  $(JUCE_OBJDIR)/AudiumApplication_b159b4c1.o \
  $(JUCE_OBJDIR)/AudiumMainWindow_9b19a902.o \
  $(JUCE_OBJDIR)/Main_70df61c1.o \
  $(JUCE_OBJDIR)/UsageAnalytics_4d1fd2d.o \
  $(JUCE_OBJDIR)/EngineAccess_b7e0d334.o \
  $(JUCE_OBJDIR)/Preferences_96bfe954.o \
  $(JUCE_OBJDIR)/AnalysisProvider_7450833c.o \
  $(JUCE_OBJDIR)/SBicSegmenter_68bb20f2.o \
  $(JUCE_OBJDIR)/AnalysisCache_e40fdde1.o \
  $(JUCE_OBJDIR)/OnsetSegmenter_cc2f2d0c.o \
  $(JUCE_OBJDIR)/BeatSegmenter_1543c9e5.o \
  $(JUCE_OBJDIR)/AnalysisWorker_87f10fc9.o \
  $(JUCE_OBJDIR)/EventMerger_5b3b38cf.o \
  $(JUCE_OBJDIR)/VoiceSource_a16dcd6e.o \
  $(JUCE_OBJDIR)/VoiceSourceContainer_3bb45bfd.o \
  $(JUCE_OBJDIR)/ClipTransportSource_2dcf5135.o \
  $(JUCE_OBJDIR)/ClipFadeSpec_6e5c9b30.o \
  $(JUCE_OBJDIR)/PlayListItemExport_8b6443f0.o \
  $(JUCE_OBJDIR)/AudioExportThread_db1a0cc7.o \
  $(JUCE_OBJDIR)/SelectionManager_445cdbb2.o \
  $(JUCE_OBJDIR)/DspClip_7236184f.o \
  $(JUCE_OBJDIR)/AutoEdit_95866c4b.o \
  $(JUCE_OBJDIR)/AutoEditParameter_df9b8e8.o \
  $(JUCE_OBJDIR)/AudioChannel_b77c3b27.o \
  $(JUCE_OBJDIR)/AudiumFactory_21005c30.o \
  $(JUCE_OBJDIR)/AudioTrack_cd929473.o \
  $(JUCE_OBJDIR)/AudioTrackContainer_546eaa98.o \
  $(JUCE_OBJDIR)/AudioTrackViewState_3eefa983.o \
  $(JUCE_OBJDIR)/AudioRegionAdapter_714661e3.o \
  $(JUCE_OBJDIR)/ResourceGroup_483fb77d.o \
  $(JUCE_OBJDIR)/LinkAudioDevice_ad84cc6b.o \
  $(JUCE_OBJDIR)/LinkEngine_c7ee2fcd.o \
  $(JUCE_OBJDIR)/AudioBusInterface_816dcfe9.o \
  $(JUCE_OBJDIR)/AudioBusRenderer_7f6f5a1d.o \
  $(JUCE_OBJDIR)/Playback_61ca68b.o \
  $(JUCE_OBJDIR)/Voice_f531b98c.o \
  $(JUCE_OBJDIR)/TransportLoop_f6f3bb5e.o \
  $(JUCE_OBJDIR)/PositionableBase_8b3640ed.o \
  $(JUCE_OBJDIR)/PlayListContainer_29209da0.o \
  $(JUCE_OBJDIR)/ClipDynamics_6197e6fd.o \
  $(JUCE_OBJDIR)/PlayListItem_8cc1eb1e.o \
  $(JUCE_OBJDIR)/PlayListScheduler_18e264da.o \
  $(JUCE_OBJDIR)/AudioRegion_38bb0c1d.o \
  $(JUCE_OBJDIR)/AudioRegionContainer_52a1322e.o \
  $(JUCE_OBJDIR)/AudioRecorder_f4310912.o \
  $(JUCE_OBJDIR)/Recording_e6186a4f.o \
  $(JUCE_OBJDIR)/RecordingActionHandler_f74d436f.o \
  $(JUCE_OBJDIR)/AudioResource_bb5eaf91.o \
  $(JUCE_OBJDIR)/AudioResourceContainer_8bac313a.o \
  $(JUCE_OBJDIR)/ChannelMapping_6a0229a8.o \
  $(JUCE_OBJDIR)/AudiumEngine_147f6ae1.o \
  $(JUCE_OBJDIR)/AudiumLookAndFeel_1d32727.o \
  $(JUCE_OBJDIR)/TrackColourLookAndFeel_7098fc17.o \
  $(JUCE_OBJDIR)/ColourIds_54ae2f37.o \
  $(JUCE_OBJDIR)/audium_AudioThumbnail_f5797f4f.o \
  $(JUCE_OBJDIR)/audium_ListBox_b1d2231e.o \
  $(JUCE_OBJDIR)/SnapToGridHandler_51ca2978.o \
  $(JUCE_OBJDIR)/ZoomHandler_aafc8870.o \
  $(JUCE_OBJDIR)/ChannelComponent_2fdebb65.o \
  $(JUCE_OBJDIR)/ChannelGroupHeaderComponent_57dc7e93.o \
  $(JUCE_OBJDIR)/ChannelsComponent_c4a4038c.o \
  $(JUCE_OBJDIR)/AudioTrackComponent_e9b51eee.o \
  $(JUCE_OBJDIR)/PlayListItemComponent_669a79e.o \
  $(JUCE_OBJDIR)/AudioTrackBaseComponent_81af44fd.o \
  $(JUCE_OBJDIR)/HeaderComponent_9265485e.o \
  $(JUCE_OBJDIR)/PlayListComponent_101ef568.o \
  $(JUCE_OBJDIR)/PlayListContainerComponent_413ee6bb.o \
  $(JUCE_OBJDIR)/RightPanelComponent_683d1bd2.o \
  $(JUCE_OBJDIR)/MainComponent_892e8d2a.o \
  $(JUCE_OBJDIR)/ArrangementModel_98e76acf.o \
  $(JUCE_OBJDIR)/AudioTrackListBoxModel_e0f725e1.o \
  $(JUCE_OBJDIR)/PlayListTableListBoxItem_a4ee4574.o \
  $(JUCE_OBJDIR)/RegionTableListBoxModel_20340f30.o \
  $(JUCE_OBJDIR)/AutoEditOverlayControl_e143b2a.o \
  $(JUCE_OBJDIR)/LoopDraggerControl_4cc280e5.o \
  $(JUCE_OBJDIR)/DraggingHandle_64fe2e5b.o \
  $(JUCE_OBJDIR)/PlayListItemDraggerControl_41919926.o \
  $(JUCE_OBJDIR)/DraggerControl_f198dea1.o \
  $(JUCE_OBJDIR)/LevelMeter_821028e9.o \
  $(JUCE_OBJDIR)/RegionLabel_5a62dea6.o \
  $(JUCE_OBJDIR)/RegionSelector_50640af7.o \
  $(JUCE_OBJDIR)/RegionTableListBox_80e14e37.o \
  $(JUCE_OBJDIR)/AudioTrackListBox_19d1f4fe.o \
  $(JUCE_OBJDIR)/ClipFadeOverlay_7793246e.o \
  $(JUCE_OBJDIR)/FadeInOutView_5098f4fc.o \
  $(JUCE_OBJDIR)/SegmentationView_81438847.o \
  $(JUCE_OBJDIR)/AutoEditPreviewView_ccfab99e.o \
  $(JUCE_OBJDIR)/WaveFormViewBase_72cde273.o \
  $(JUCE_OBJDIR)/AudioClipView_1a258175.o \
  $(JUCE_OBJDIR)/BinaryData_ce4232d4.o \
  $(JUCE_OBJDIR)/include_juce_analytics_f8e9fa94.o \
  $(JUCE_OBJDIR)/include_juce_audio_basics_8a4e984a.o \
  $(JUCE_OBJDIR)/include_juce_audio_devices_63111d02.o \
  $(JUCE_OBJDIR)/include_juce_audio_formats_15f82001.o \
  $(JUCE_OBJDIR)/include_juce_audio_processors_10c03666.o \
  $(JUCE_OBJDIR)/include_juce_audio_processors_headless_1902bafc.o \
  $(JUCE_OBJDIR)/include_juce_audio_processors_headless_ara_8deeb88d.o \
  $(JUCE_OBJDIR)/include_juce_audio_processors_headless_lv2_libs_36180e32.o \
  $(JUCE_OBJDIR)/include_juce_audio_utils_9f9fb2d6.o \
  $(JUCE_OBJDIR)/include_juce_core_f26d17db.o \
  $(JUCE_OBJDIR)/include_juce_core_CompilationTime_9257742c.o \
  $(JUCE_OBJDIR)/include_juce_data_structures_7471b1e3.o \
  $(JUCE_OBJDIR)/include_juce_dsp_aeb2060f.o \
  $(JUCE_OBJDIR)/include_juce_events_fd7d695.o \
  $(JUCE_OBJDIR)/include_juce_graphics_f817e147.o \
  $(JUCE_OBJDIR)/include_juce_graphics_Harfbuzz_60c52ba2.o \
  $(JUCE_OBJDIR)/include_juce_graphics_Sheenbidi_c310974d.o \
  $(JUCE_OBJDIR)/include_juce_gui_basics_e3f79785.o \
  $(JUCE_OBJDIR)/include_juce_gui_extra_6dee1c1a.o \

.PHONY: clean all strip

all : $(JUCE_OUTDIR)/$(JUCE_TARGET_APP)

$(JUCE_OUTDIR)/$(JUCE_TARGET_APP) : $(OBJECTS_APP) $(JUCE_OBJDIR)/execinfo.cmd $(RESOURCES)
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo >&2 "pkg-config not installed. Please, install it."; exit 1; }
	@$(PKG_CONFIG) --print-errors alsa freetype2 fontconfig libcurl
	@echo Linking "Audionaut - App"
	-$(V_AT)mkdir -p $(JUCE_BINDIR)
	-$(V_AT)mkdir -p $(JUCE_LIBDIR)
	-$(V_AT)mkdir -p $(JUCE_OUTDIR)
	$(V_AT)$(CXX) -o $(JUCE_OUTDIR)/$(JUCE_TARGET_APP) $(OBJECTS_APP) $(JUCE_LDFLAGS) $(shell cat $(JUCE_OBJDIR)/execinfo.cmd) $(JUCE_LDFLAGS_APP) $(RESOURCES) $(TARGET_ARCH)

$(JUCE_OBJDIR)/AudiumMenuModel_e3ecba7b.o: ../../Source/Application/AudiumMenuModel.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudiumMenuModel.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudiumApplication_b159b4c1.o: ../../Source/Application/AudiumApplication.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudiumApplication.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudiumMainWindow_9b19a902.o: ../../Source/Application/AudiumMainWindow.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudiumMainWindow.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Main_70df61c1.o: ../../Source/Application/Main.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Main.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/UsageAnalytics_4d1fd2d.o: ../../Source/Application/UsageAnalytics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling UsageAnalytics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/EngineAccess_b7e0d334.o: ../../Source/Util/EngineAccess.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling EngineAccess.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Preferences_96bfe954.o: ../../Source/Util/Preferences.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Preferences.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AnalysisProvider_7450833c.o: ../../Source/Engine/Analysis/AnalysisProvider.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AnalysisProvider.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/SBicSegmenter_68bb20f2.o: ../../Source/Engine/Analysis/SBicSegmenter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling SBicSegmenter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AnalysisCache_e40fdde1.o: ../../Source/Engine/Analysis/AnalysisCache.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AnalysisCache.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/OnsetSegmenter_cc2f2d0c.o: ../../Source/Engine/Analysis/OnsetSegmenter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling OnsetSegmenter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/BeatSegmenter_1543c9e5.o: ../../Source/Engine/Analysis/BeatSegmenter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling BeatSegmenter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AnalysisWorker_87f10fc9.o: ../../Source/Engine/Analysis/AnalysisWorker.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AnalysisWorker.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/EventMerger_5b3b38cf.o: ../../Source/Engine/Analysis/EventMerger.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling EventMerger.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/VoiceSource_a16dcd6e.o: ../../Source/Engine/AudioSources/VoiceSource.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling VoiceSource.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/VoiceSourceContainer_3bb45bfd.o: ../../Source/Engine/AudioSources/VoiceSourceContainer.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling VoiceSourceContainer.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ClipTransportSource_2dcf5135.o: ../../Source/Engine/AudioSources/ClipTransportSource.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ClipTransportSource.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ClipFadeSpec_6e5c9b30.o: ../../Source/Engine/AudioSources/ClipFadeSpec.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ClipFadeSpec.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListItemExport_8b6443f0.o: ../../Source/Engine/Export/PlayListItemExport.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListItemExport.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioExportThread_db1a0cc7.o: ../../Source/Engine/Export/AudioExportThread.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioExportThread.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/SelectionManager_445cdbb2.o: ../../Source/Engine/Selection/SelectionManager.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling SelectionManager.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/DspClip_7236184f.o: ../../Source/Engine/Core/DspClip.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling DspClip.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AutoEdit_95866c4b.o: ../../Source/Engine/AutoEdit/AutoEdit.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AutoEdit.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AutoEditParameter_df9b8e8.o: ../../Source/Engine/AutoEdit/AutoEditParameter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AutoEditParameter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioChannel_b77c3b27.o: ../../Source/Engine/Channel/AudioChannel.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioChannel.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudiumFactory_21005c30.o: ../../Source/Engine/Factory/AudiumFactory.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudiumFactory.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioTrack_cd929473.o: ../../Source/Engine/Group/AudioTrack.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioTrack.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioTrackContainer_546eaa98.o: ../../Source/Engine/Group/AudioTrackContainer.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioTrackContainer.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioTrackViewState_3eefa983.o: ../../Source/Engine/Group/AudioTrackViewState.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioTrackViewState.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioRegionAdapter_714661e3.o: ../../Source/Engine/Group/AudioRegionAdapter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioRegionAdapter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ResourceGroup_483fb77d.o: ../../Source/Engine/Group/ResourceGroup.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ResourceGroup.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/LinkAudioDevice_ad84cc6b.o: ../../Source/Engine/Link/LinkAudioDevice.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling LinkAudioDevice.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/LinkEngine_c7ee2fcd.o: ../../Source/Engine/Link/LinkEngine.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling LinkEngine.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioBusInterface_816dcfe9.o: ../../Source/Engine/Playback/AudioBusInterface.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioBusInterface.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioBusRenderer_7f6f5a1d.o: ../../Source/Engine/Playback/AudioBusRenderer.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioBusRenderer.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Playback_61ca68b.o: ../../Source/Engine/Playback/Playback.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Playback.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Voice_f531b98c.o: ../../Source/Engine/Playback/Voice.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Voice.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/TransportLoop_f6f3bb5e.o: ../../Source/Engine/PlayList/TransportLoop.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling TransportLoop.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PositionableBase_8b3640ed.o: ../../Source/Engine/PlayList/PositionableBase.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PositionableBase.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListContainer_29209da0.o: ../../Source/Engine/PlayList/PlayListContainer.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListContainer.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ClipDynamics_6197e6fd.o: ../../Source/Engine/PlayList/ClipDynamics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ClipDynamics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListItem_8cc1eb1e.o: ../../Source/Engine/PlayList/PlayListItem.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListItem.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListScheduler_18e264da.o: ../../Source/Engine/PlayList/PlayListScheduler.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListScheduler.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioRegion_38bb0c1d.o: ../../Source/Engine/Region/AudioRegion.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioRegion.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioRegionContainer_52a1322e.o: ../../Source/Engine/Region/AudioRegionContainer.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioRegionContainer.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioRecorder_f4310912.o: ../../Source/Engine/Recording/AudioRecorder.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioRecorder.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/Recording_e6186a4f.o: ../../Source/Engine/Recording/Recording.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling Recording.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/RecordingActionHandler_f74d436f.o: ../../Source/Engine/Recording/RecordingActionHandler.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling RecordingActionHandler.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioResource_bb5eaf91.o: ../../Source/Engine/Resource/AudioResource.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioResource.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioResourceContainer_8bac313a.o: ../../Source/Engine/Resource/AudioResourceContainer.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioResourceContainer.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ChannelMapping_6a0229a8.o: ../../Source/Engine/Resource/ChannelMapping.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ChannelMapping.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudiumEngine_147f6ae1.o: ../../Source/Engine/AudiumEngine.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudiumEngine.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudiumLookAndFeel_1d32727.o: ../../Source/Interface/LookAndFeel/AudiumLookAndFeel.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudiumLookAndFeel.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/TrackColourLookAndFeel_7098fc17.o: ../../Source/Interface/LookAndFeel/TrackColourLookAndFeel.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling TrackColourLookAndFeel.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ColourIds_54ae2f37.o: ../../Source/Interface/ColourIds.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ColourIds.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/audium_AudioThumbnail_f5797f4f.o: ../../Source/Interface/Widgets/audium_AudioThumbnail.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling audium_AudioThumbnail.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/audium_ListBox_b1d2231e.o: ../../Source/Interface/Widgets/audium_ListBox.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling audium_ListBox.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/SnapToGridHandler_51ca2978.o: ../../Source/Interface/Handlers/SnapToGridHandler.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling SnapToGridHandler.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ZoomHandler_aafc8870.o: ../../Source/Interface/Handlers/ZoomHandler.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ZoomHandler.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ChannelComponent_2fdebb65.o: ../../Source/Interface/Components/MiddlePanel/ChannelView/ChannelComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ChannelComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ChannelGroupHeaderComponent_57dc7e93.o: ../../Source/Interface/Components/MiddlePanel/ChannelView/ChannelGroupHeaderComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ChannelGroupHeaderComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ChannelsComponent_c4a4038c.o: ../../Source/Interface/Components/MiddlePanel/ChannelView/ChannelsComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ChannelsComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioTrackComponent_e9b51eee.o: ../../Source/Interface/Components/MiddlePanel/ArrangementView/AudioTrackComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioTrackComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListItemComponent_669a79e.o: ../../Source/Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListItemComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioTrackBaseComponent_81af44fd.o: ../../Source/Interface/Components/MiddlePanel/AudioTrackBaseComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioTrackBaseComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/HeaderComponent_9265485e.o: ../../Source/Interface/Components/HeaderPanel/HeaderComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling HeaderComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListComponent_101ef568.o: ../../Source/Interface/Components/RightPanel/PlayListComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListContainerComponent_413ee6bb.o: ../../Source/Interface/Components/RightPanel/PlayListContainerComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListContainerComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/RightPanelComponent_683d1bd2.o: ../../Source/Interface/Components/RightPanel/RightPanelComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling RightPanelComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/MainComponent_892e8d2a.o: ../../Source/Interface/Components/MainComponent.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling MainComponent.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ArrangementModel_98e76acf.o: ../../Source/Interface/Models/ArrangementModel.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ArrangementModel.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioTrackListBoxModel_e0f725e1.o: ../../Source/Interface/Models/AudioTrackListBoxModel.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioTrackListBoxModel.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListTableListBoxItem_a4ee4574.o: ../../Source/Interface/Models/PlayListTableListBoxItem.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListTableListBoxItem.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/RegionTableListBoxModel_20340f30.o: ../../Source/Interface/Models/RegionTableListBoxModel.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling RegionTableListBoxModel.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AutoEditOverlayControl_e143b2a.o: ../../Source/Interface/Controls/AutoEditOverlayControl.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AutoEditOverlayControl.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/LoopDraggerControl_4cc280e5.o: ../../Source/Interface/Controls/LoopDraggerControl.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling LoopDraggerControl.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/DraggingHandle_64fe2e5b.o: ../../Source/Interface/Controls/DraggingHandle.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling DraggingHandle.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/PlayListItemDraggerControl_41919926.o: ../../Source/Interface/Controls/PlayListItemDraggerControl.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling PlayListItemDraggerControl.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/DraggerControl_f198dea1.o: ../../Source/Interface/Controls/DraggerControl.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling DraggerControl.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/LevelMeter_821028e9.o: ../../Source/Interface/Controls/LevelMeter.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling LevelMeter.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/RegionLabel_5a62dea6.o: ../../Source/Interface/Controls/RegionLabel.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling RegionLabel.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/RegionSelector_50640af7.o: ../../Source/Interface/Controls/RegionSelector.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling RegionSelector.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/RegionTableListBox_80e14e37.o: ../../Source/Interface/Controls/RegionTableListBox.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling RegionTableListBox.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioTrackListBox_19d1f4fe.o: ../../Source/Interface/Controls/AudioTrackListBox.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioTrackListBox.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/ClipFadeOverlay_7793246e.o: ../../Source/Interface/Views/ClipFadeOverlay.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling ClipFadeOverlay.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/FadeInOutView_5098f4fc.o: ../../Source/Interface/Views/FadeInOutView.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling FadeInOutView.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/SegmentationView_81438847.o: ../../Source/Interface/Views/SegmentationView.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling SegmentationView.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AutoEditPreviewView_ccfab99e.o: ../../Source/Interface/Views/AutoEditPreviewView.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AutoEditPreviewView.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/WaveFormViewBase_72cde273.o: ../../Source/Interface/Views/WaveFormViewBase.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling WaveFormViewBase.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/AudioClipView_1a258175.o: ../../Source/Interface/Views/AudioClipView.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling AudioClipView.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/BinaryData_ce4232d4.o: ../../JuceLibraryCode/BinaryData.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling BinaryData.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_analytics_f8e9fa94.o: ../../JuceLibraryCode/include_juce_analytics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_analytics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_basics_8a4e984a.o: ../../JuceLibraryCode/include_juce_audio_basics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_basics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_devices_63111d02.o: ../../JuceLibraryCode/include_juce_audio_devices.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_devices.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_formats_15f82001.o: ../../JuceLibraryCode/include_juce_audio_formats.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_formats.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_processors_10c03666.o: ../../JuceLibraryCode/include_juce_audio_processors.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_processors.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_processors_headless_1902bafc.o: ../../JuceLibraryCode/include_juce_audio_processors_headless.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_processors_headless.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_processors_headless_ara_8deeb88d.o: ../../JuceLibraryCode/include_juce_audio_processors_headless_ara.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_processors_headless_ara.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_processors_headless_lv2_libs_36180e32.o: ../../JuceLibraryCode/include_juce_audio_processors_headless_lv2_libs.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_processors_headless_lv2_libs.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_audio_utils_9f9fb2d6.o: ../../JuceLibraryCode/include_juce_audio_utils.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_audio_utils.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_core_f26d17db.o: ../../JuceLibraryCode/include_juce_core.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_core.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_core_CompilationTime_9257742c.o: ../../JuceLibraryCode/include_juce_core_CompilationTime.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_core_CompilationTime.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_data_structures_7471b1e3.o: ../../JuceLibraryCode/include_juce_data_structures.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_data_structures.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_dsp_aeb2060f.o: ../../JuceLibraryCode/include_juce_dsp.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_dsp.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_events_fd7d695.o: ../../JuceLibraryCode/include_juce_events.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_events.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_graphics_f817e147.o: ../../JuceLibraryCode/include_juce_graphics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_graphics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_graphics_Harfbuzz_60c52ba2.o: ../../JuceLibraryCode/include_juce_graphics_Harfbuzz.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_graphics_Harfbuzz.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_graphics_Sheenbidi_c310974d.o: ../../JuceLibraryCode/include_juce_graphics_Sheenbidi.c
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_graphics_Sheenbidi.c"
	$(V_AT)$(CC) $(JUCE_CFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_gui_basics_e3f79785.o: ../../JuceLibraryCode/include_juce_gui_basics.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_gui_basics.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/include_juce_gui_extra_6dee1c1a.o: ../../JuceLibraryCode/include_juce_gui_extra.cpp
	-$(V_AT)mkdir -p $(@D)
	@echo "Compiling include_juce_gui_extra.cpp"
	$(V_AT)$(CXX) $(JUCE_CXXFLAGS) $(JUCE_CPPFLAGS_APP) $(JUCE_CFLAGS_APP) -o "$@" -c "$<"

$(JUCE_OBJDIR)/execinfo.cmd:
	-$(V_AT)mkdir -p $(@D)
	-@if [ -z "$(V_AT)" ]; then echo "Checking if we need to link libexecinfo"; fi
	$(V_AT)printf "int main() { return 0; }" | $(CXX) -x c++ -o $(@D)/execinfo.x -lexecinfo - >/dev/null 2>&1 && printf -- "-lexecinfo" > "$@" || touch "$@"

$(JUCE_OBJDIR)/cxxfs.cmd:
	-$(V_AT)mkdir -p $(@D)
	-@if [ -z "$(V_AT)" ]; then echo "Checking if we need to link stdc++fs"; fi
	$(V_AT)printf "int main() { return 0; }" | $(CXX) -x c++ -o $(@D)/cxxfs.x -lstdc++fs - >/dev/null 2>&1 && printf -- "-lstdc++fs" > "$@" || touch "$@"

clean:
	@echo Cleaning Audionaut
	$(V_AT)$(CLEANCMD)

strip:
	@echo Stripping Audionaut
	-$(V_AT)$(STRIP) --strip-unneeded $(JUCE_OUTDIR)/$(JUCE_TARGET_APP)

-include $(OBJECTS_APP:%.o=%.d)
