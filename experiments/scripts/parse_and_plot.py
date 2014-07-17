#! /usr/bin/env python2.7

import matplotlib.pyplot as plt
import matplotlib.mlab as mlab
import numpy as np
from scipy.stats import norm
from scipy.interpolate import interp1d
import random

def open_file(filename):
        with open(filename) as f:
                lines = list(f)
                lines = lines[1:-1]
                lines.pop()
                lines.pop()
                return lines

def get_nums(lines):
        nums = []
        for line in lines:
                splt = line.split(' ')
                nums.append(float(splt[-3]))
        return nums

def plot_dist(clean, instr):
        normf = 1
        fig = plt.figure()
        clean = np.array(clean)
        instr = np.array(instr)
        clean = clean * normf;
        instr = instr * normf;

        clean_med = np.median(clean)
        print clean_med
        instr_med = np.median(instr)

        overhead_med = ((instr_med - clean_med) / clean_med) * 100

        clean_avg = np.average(clean)
        instr_avg = np.average(instr)
        overhead_avg = ((instr_avg - clean_avg) / clean_avg) * 100

        clean_nn = np.percentile(clean, 99)
        instr_nn = np.percentile(instr, 99)
        overhead_nn = ((instr_nn - clean_nn) / clean_nn) * 100
        textstr = '$\Delta\mathrm{median }=%.2f$%%\n$\Delta\mathrm{average }=%.2f$%%\n$\Delta\mathrm{99^{th}percentile }=%.2f$%%'%(overhead_med, overhead_avg, overhead_nn)

        subfig = fig.add_subplot(111)
        clean_n, clean_bins, clean_patches = subfig.hist(clean, 1000, color='green', normed=True, label='lighttpd', alpha=0.8)
        c_bsz = clean_bins[2] - clean_bins[1]
        instr_range = np.max(instr) - np.min(instr)
        instr_bins_nr = int(np.ceil(instr_range / c_bsz))
        instr_n, instr_bins, instr_patches = subfig.hist(instr, instr_bins_nr, color='blue', normed=True, label='lighttpd + Resourceful', alpha=0.6)
        subfig.set_xlim(2,5)
        subfig.set_ylim(0,3.5)
        #fig.delaxes(subfig)

        #edge_ix = 500

        #subfig_2 = fig.add_subplot(111)
        #clean_bins = clean_bins[0: edge_ix]
        ##print clean_bins
        #clean_n = clean_n[0: edge_ix]
        #clean_bins = clean_bins + c_bsz/float(2)
        ##clean_bins = sorted(clean_bins)
        #clean_z = interp1d(clean_bins, clean_n, kind='cubic')
        #clean_xp = np.linspace(min(clean_bins), max(clean_bins), 4000)

        #instr_bins = instr_bins[0: edge_ix]
        #instr_n = instr_n[0: edge_ix]
        #print (len(instr_n), len(instr_bins))
        #instr_bins = instr_bins + c_bsz/float(2)
        ###instr_bins = sorted(instr_bins)
        #instr_z = interp1d(instr_bins, instr_n, kind='cubic')
        #instr_xp = np.linspace(min(instr_bins), max(instr_bins), 4000)

        #subfig_2.plot(clean_xp, clean_z(clean_xp), 'g-', linewidth=1.3, label='lighttpd')
        #subfig_2.plot(instr_xp, instr_z(instr_xp), 'b-', linewidth=1.5, label='lighttpd + Resourceful')
        ##subfig_2.set_xlim(2,4)
        ##subfig_2.set_ylim(0,1)
        subfig.legend(loc="upper left")

        subfig.text(0.98, 0.97, textstr, transform=subfig.transAxes,
                      verticalalignment='top', horizontalalignment='right', bbox=dict(fc="w", pad=10), fontsize=17)
        plt.xlabel('Latency (ms)', fontsize=17)
        plt.ylabel('Frequency Density', fontsize=17)

        plt.show()

def plot_box(clean, instr):
        data = [clean, instr]
        plt.boxplot(data)
        plt.show()

def print_info(clean, instr):
        clean_med = np.median(clean)
        instr_med = np.median(instr)

        print 'Clean Median: ' + str(clean_med)
        print 'Instr Median: ' + str(instr_med)

        overhead_med = ((instr_med - clean_med) / clean_med) * 100
        print 'Overhead Median: ' + str(overhead_med) + '%'

        clean_avg = np.average(clean)
        instr_avg = np.average(instr)
        overhead_avg = ((instr_avg - clean_avg) / clean_avg) * 100
        print 'Overhead Average: ' + str(overhead_avg) + '%'

        clean_nn = np.percentile(clean, 99)
        instr_nn = np.percentile(instr, 99)
        overhead_nn = ((instr_nn - clean_nn) / clean_nn) * 100
        print 'Overhead NN: ' + str(overhead_nn) + '%'

if __name__ == '__main__':
        clean_lines = open_file('../raw_data/clean_10000.txt')
        instr_lines = open_file('../raw_data/instrumented_10000.txt')

        clean = get_nums(clean_lines)
        instr = get_nums(instr_lines)

        print_info(clean, instr)

        plot_dist(clean, instr)
        #plot_box(clean, instr)
