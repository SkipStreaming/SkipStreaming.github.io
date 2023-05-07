import getopt
import sys
import librosa
import os
import shutil
from pathlib import Path
import subprocess
import numpy as np
import sklearn
import sklearn.cluster
import sklearn.feature_extraction
import sklearn.neighbors

series_dir = ''

n_fft = 8192
hop_length = 4096

def get_audio_codec(filename):
    cmd = 'ffprobe -v error -select_streams a:0 -show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 %s' % filename
    return subprocess.getoutput(cmd)


def build_scene_tree(filename, out_filename):
    y, sr = librosa.load(filename, sr=None)
    mfccs = librosa.feature.melspectrogram(y=y, sr=sr, n_fft=n_fft, hop_length=hop_length)
    mfccs = librosa.power_to_db(mfccs, ref=np.max)

    data = np.atleast_2d(mfccs)
    data = np.swapaxes(data, -1, 0)
    n = data.shape[0]
    data = data.reshape((n, -1), order="F")
    grid = sklearn.feature_extraction.image.grid_to_graph(n_x=n, n_y=1, n_z=1)
    clusterer = sklearn.cluster.AgglomerativeClustering(compute_full_tree=True, connectivity=grid)
    clusterer.fit(data)

    # sklearn.cluster._agglomerative._hc_cut
    # sklearn.cluster._agglomerative._hc_cut(clusterer.n_clusters_, clusterer.children_,
    #                                clusterer.n_leaves_)

    # out_data = np.array([n, sr, n_fft, hop_length])
    out_data = np.column_stack((np.arange(clusterer.children_.shape[0]), clusterer.children_))

    np.savetxt(out_filename, out_data, fmt="%d", header='%d %d %d %d' % (n, sr, n_fft, hop_length))

def demux_audio(dir):
    filenames = os.listdir(dir)
    if not os.path.exists('scene_tree'):
        os.mkdir('scene_tree')
    if os.path.exists('audio'):
        shutil.rmtree('audio')
    os.mkdir('audio')
    for filename in filenames:
        input_video_filename = os.path.join(dir, filename)
        format = get_audio_codec(input_video_filename)
        print(filename)
        output_audio_filename = os.path.join('audio', Path(filename).with_suffix('.' + 'wav'))
        cmd = 'ffmpeg -v error -i %s -vn %s' % (input_video_filename, output_audio_filename)
        os.system(cmd)
        output_scene_filename = os.path.join('scene_tree', Path(filename).with_suffix('.txt'))
        build_scene_tree(output_audio_filename, output_scene_filename)


def main(argv):
    global series_dir
    try:
        options, args = getopt.getopt(argv, 'i:o:f:h:')
    except getopt.GetoptError:
        sys.exit()

    for option, value in options:
        if option == '-i':
            series_dir = value
    demux_audio(series_dir)

if __name__ == '__main__':
    main(sys.argv[1:])