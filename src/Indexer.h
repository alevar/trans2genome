//
//  Indexer.h
//  TopHat
//
//  Created by Ales Varabyou on 07/26/19 based on gtf_to_fasta from Tophat written by Harold Pimentel
//

#ifndef INDEXER_H
#define INDEXER_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>

#include "gff.h"
#include "GFaSeqGet.h"
#include "FastaTools.h"
#include "arg_parse.h"
#include "Multimap.hh"

class Indexer {
public:
    Indexer(std::string gtf_fname, std::string genome_fname,const std::string& out_fname, int kmer_length,bool multi);
    ~Indexer();
    void make_transcriptome();

    // for debugging
    void print_mapping();
    void print_mmap();
    void print_mmap_multi();

    void save(bool multi, bool uniq);
private:
    GffReader gtfReader_;

    std::string gtf_fname_;
    std::string genome_fname_;
    FILE* gtf_fhandle_;

    bool multi = false; // multimapper resolution
    int kmerlen; // kmer length to index
    std::string out_fname; // base name for all files
    int topTransID = 0; // the highest transcript ID assigned for any transcript in the current transcriptome. This information is written to the info file

    std::string trans_fastaname;
    std::string info_fname,tlst_fname,multimap_fname,unique_fname,gene_fname,genome_headername,tgmap_fname,gnamemap_fname,tnamemap_fname;

    void transcript_map();
    std::string get_exonic_sequence(GffObj& p_trans, FastaRecord& rec, std::string& coords,int transID);
    std::string get_rc_exonic_sequence(GffObj& p_trans, FastaRecord& rec, std::string& coords,int transID);
    void add_to_geneMap(GffObj &p_trans);
    void add_to_refMap(GffObj &p_trans,int contig_len);
    void save_header();

    Indexer() = default; // Don't want anyone calling the constructor w/o options

    // MULTIMAPPERS
    Multimap mmap;

    std::unordered_map<int,const char* const> int_to_chr; // conversion back from intigers to chromosomes
    std::unordered_map<const char*,int> chr_to_int; // conversion table from chromosome id to int

    typedef std::map<std::string, std::vector< int >* > ContigTransMap;
    ContigTransMap contigTransMap_;

    int curGeneID = 0;
    // the structure of the gene tuple is as follows:
    // 1. gene id
    // 2. strand
    // 3. start
    // 4. end
    // 5. effective length
    std::map<std::string,Gene> geneMap; // stores minimum and maximum gene coordinates of transcripts in a given gene as well as strand and unique ID, also stores gene identifier as well as the effective length
    std::pair< std::map<
            std::string,
            Gene >::iterator,bool> exists_cur_gene;
    std::map<std::string,Gene>::iterator found_gene;

    std::map<std::string,std::string> gid2name,gname2id;
    std::pair<std::map<std::string,std::string>::iterator,bool> gn_it;

    std::map<int,std::pair<std::string,int>> id_to_ref; // this is used ot build a header for the genomic file. contains the index (line in header), ref name and length
    std::map<std::string,int> id_to_ref_no_trans; // to append at the end

    // TODO: needs to handle reverse complement of a kmer for the multimapper index
};

#endif
