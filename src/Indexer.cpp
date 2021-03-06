#include <utility>
#include "Indexer.h"

//
// Created by Ales Varabyou on 6/16/19.
//

std::string Indexer::get_exonic_sequence(GffObj &p_trans,FastaRecord &rec, std::string& coords,int transID){
    GList<GffExon>& exon_list = p_trans.exons;

    std::string exon_seq;
    size_t length=0;
    coords.clear();
    std::stringstream ss;

    for (int i = 0; i < exon_list.Count(); ++i) {
        GffExon& cur_exon = *(exon_list.Get(i));
        length = (cur_exon.end+1) - cur_exon.start;

        exon_seq += rec.seq_.substr(cur_exon.start - 1, length);
        ss << ',' << cur_exon.start << '_' << cur_exon.end;
    }

    // get coordinates into the map
    if (exon_seq.size()>=this->kmerlen and this->multi){ // sanity check for 0 and 1 base exons
        this->found_gene = this->geneMap.find(std::string(p_trans.getGeneID()));
        if(this->found_gene != this->geneMap.end()){
            this->mmap.add_sequence(exon_seq,p_trans,this->found_gene->second.get_locid(),transID);
        }
        else{

            std::cerr<<"@ERROR::something went wrong with gene ID assignment"<<std::endl;
            std::cerr<<"@ERROR::looking up: "<<p_trans.getGeneID()<<"\t"<<std::string(p_trans.getGeneID())<<std::endl;
            exit(1);
        }
    }

    coords = ss.str().substr(1);
    return exon_seq;
}

std::string Indexer::get_rc_exonic_sequence(GffObj &p_trans,FastaRecord &rec, std::string& coords,int transID){
    GList<GffExon>& exon_list = p_trans.exons;

    std::string exon_seq;
    size_t length=0;
    coords.clear();
    std::stringstream ss;

    for (int i = 0; i < exon_list.Count(); ++i) {
        GffExon& cur_exon = *(exon_list.Get(i));
        length = (cur_exon.end+1) - cur_exon.start;

        exon_seq += rec.seq_.substr(cur_exon.start - 1, length);
        ss << ',' << cur_exon.start << '_' << cur_exon.end;
    }

    // get coordinates into the map
    if (exon_seq.size()>=this->kmerlen and this->multi){ // sanity check for 0 and 1 base exons
        this->found_gene = this->geneMap.find(std::string(p_trans.getGeneID()));
        if(this->found_gene != this->geneMap.end()){
            this->mmap.add_rc_sequence(exon_seq,p_trans,this->found_gene->second.get_locid(),transID);
        }
        else{
            std::cerr<<"@ERROR::something went wrong with gene ID assignment"<<std::endl;
            std::cerr<<"@ERROR::looking up: "<<p_trans.getGeneID()<<"\t"<<std::string(p_trans.getGeneID())<<std::endl;
            exit(1);
        }
    }

    coords = ss.str().substr(1);
    return exon_seq;
}

Indexer::Indexer(std::string gtf_fname, std::string genome_fname,const std::string& out_fname, int kmerlen,bool multi){
    gtf_fname_ = gtf_fname;
    gtf_fhandle_ = fopen(gtf_fname_.c_str(), "r");
    if (gtf_fhandle_ == nullptr)
    {
        std::cerr << "@ERROR::Couldn't open annotation: " << gtf_fname_<< std::endl;
        exit(1);
    }
    std::cerr << "@LOG::Reading the annotation file: " << gtf_fname_ << std::endl;
    gtfReader_.init(gtf_fhandle_, true); // load recognizable transcript features only
    gtfReader_.readAll();
    std::cerr << "@LOG::loaded the annotation"<<std::endl;

    this->tlst_fname = out_fname;
    this->tlst_fname.append(".tlst");

    this->gene_fname = out_fname;
    this->gene_fname.append(".glst");

    this->info_fname = out_fname;
    this->info_fname.append(".info");

    this->trans_fastaname = out_fname;
    this->trans_fastaname.append(".fasta");

    this->genome_headername = out_fname;
    this->genome_headername.append(".genome_header");

    this->tgmap_fname = out_fname;
    this->tgmap_fname.append(".tgmap"); // used as a map for salmon estimation

    this->gnamemap_fname = out_fname;
    this->gnamemap_fname.append(".gnamemap"); // used as a map for salmon estimation

    this->tnamemap_fname = out_fname;
    this->tnamemap_fname.append(".tnamemap"); // used as a map for salmon estimation

    genome_fname_ = std::move(genome_fname);

    this->multi =  multi;
    this->kmerlen = kmerlen;
    this->mmap.set_kmerlen(kmerlen);
    this->out_fname = out_fname;

    // Make a map from the GffObj
    transcript_map();
}

Indexer::~Indexer(){
    ContigTransMap::iterator it;
    for (it = contigTransMap_.begin(); it != contigTransMap_.end(); ++it) {
        delete it->second;
    }
}

void Indexer::add_to_geneMap(GffObj &p_trans){
    // populate the genemap
    int nst=p_trans.start;
    int nen=p_trans.end;
    char* geneID=p_trans.getGeneID();
    char* geneName=p_trans.getGeneName();
    if(geneID==nullptr && geneName==nullptr){
        std::cerr<<"@ERROR::wrong gene: "<<p_trans.gseq_id<<"\t"<<p_trans.getID()<<std::endl;
        exit(1);
    }
    else if(geneID==nullptr){
        std::cerr<<"no gene id found"<<std::endl;
        exit(-1);
    }
    else if(geneName==nullptr){
        std::cerr<<"no gene name found"<<std::endl;
        exit(-1);
    }
    else{
        // map ID to name
        this->gid2name.insert(std::make_pair(std::string(geneID),std::string(geneName)));
        this->gname2id.insert(std::make_pair(std::string(geneName),std::string(geneID)));

        exists_cur_gene = this->geneMap.insert(std::make_pair(std::string(geneID),Gene(this->curGeneID,p_trans)));
        if(!exists_cur_gene.second){ // the key did exist - update start and end and add new exons
            exists_cur_gene.first->second.add_transcript(p_trans);
        }
        else{ // saw a new gene, so need to create a new identifier
            this->curGeneID++;
        }
    }
}

void Indexer::add_to_refMap(GffObj &p_trans,int contig_len){
    // populate the refmap
    this->id_to_ref.insert(std::make_pair(p_trans.gseq_id,std::make_pair(p_trans.getRefName(),contig_len)));
}

void Indexer::make_transcriptome(){
    std::vector<int> *p_contig_vec;

    FastaReader fastaReader(genome_fname_);
    FastaWriter fastaWriter(this->trans_fastaname);
    FastaRecord cur_contig;

    std::cerr<<"@LOG::begin parsing transcriptome"<<std::endl;

    std::ofstream tlst(this->tlst_fname);
    std::ofstream tgmap(this->tgmap_fname);
    std::ofstream gnamemap(this->gnamemap_fname);
    std::ofstream tnamemap(this->tnamemap_fname);

    while (fastaReader.good()) {
        fastaReader.next(cur_contig);
        // If this contig isn't in the map, then there are no transcripts
        // associated with it. Skip it.
        if (contigTransMap_.find(cur_contig.id_) ==
            contigTransMap_.end()){
            // no transcripts are found - contig needs to be added at the end of the BAM header just in case
            this->id_to_ref_no_trans.insert(std::make_pair(cur_contig.id_,cur_contig.seq_.size()));
            continue;
        }

        p_contig_vec = contigTransMap_[cur_contig.id_];

        FastaRecord out_rec;
        for (int trans_idx : *p_contig_vec) {
            GffObj *p_trans = gtfReader_.gflst.Get(trans_idx);
            add_to_refMap(*p_trans,cur_contig.seq_.size());

            // add the gene record to the geneMap first
            this->add_to_geneMap(*p_trans);

            std::string coordstr;
            out_rec.seq_ = get_exonic_sequence(*p_trans,cur_contig, coordstr,trans_idx);
            if (out_rec.seq_.empty());
            std::stringstream ss;
            ss << trans_idx;
            out_rec.id_ = ss.str();
            if(std::stoi(out_rec.id_)>this->topTransID){ // check if current id is greater than the highest previously observed
                topTransID = std::stoi(out_rec.id_);
            }
            out_rec.desc_="";
            this->found_gene = this->geneMap.find(p_trans->getGeneID());
            if(this->found_gene == this->geneMap.end()) { // gene not found
                std::cerr << "@ERROR::an error in GeneID ocurred" << std::endl;
                exit(-1);
            }
            out_rec.desc_.append(std::to_string(this->found_gene->second.get_locid()));
            out_rec.desc_.push_back('@');
            out_rec.desc_.append(std::to_string(p_trans->gseq_id));
            out_rec.desc_.push_back(p_trans->strand);
            out_rec.desc_.append(coordstr); //list of exon coordinates
            tlst << out_rec.id_ << '\t' << out_rec.desc_ << std::endl;
            tlst.flush();
            tgmap << trans_idx <<"\t"<<found_gene->second.get_locid()<<std::endl;
            tnamemap<<trans_idx<<"\t"<<p_trans->getID()<<std::endl;
            fastaWriter.write(out_rec);
        }
    }

    FastaReader fastaReader4rc(genome_fname_);
    FastaRecord cur_rc_contig;

    while (fastaReader4rc.good()) {
        fastaReader4rc.next(cur_rc_contig);

        if (contigTransMap_.find(cur_rc_contig.id_) == contigTransMap_.end()){
            continue;
        }

        p_contig_vec = contigTransMap_[cur_rc_contig.id_];

        for (int trans_idx : *p_contig_vec) {
            GffObj *p_trans = gtfReader_.gflst.Get(trans_idx);

            std::string coordstr;
            get_rc_exonic_sequence(*p_trans,cur_rc_contig, coordstr,trans_idx); // can ignore the return since already been written
        }
    }

    // TODO: include reverse strand in the multimpper index - needs to be handled correctly when evaluating multimappers
    //     also needs to be reflected in the flags when performing evaluation
    tlst.close();
    tgmap.close();
    for(auto& gv : this->geneMap){
        gnamemap<<gv.second.get_locid()<<"\t"<<gv.first<<std::endl;
    }
    gnamemap.close();
    std::cerr<<"@LOG::done parsing transcriptome"<<std::endl;

    std::cerr<<"@LOG::writing gene information"<<std::endl;
    std::ofstream genefp(this->gene_fname);
    // write genes to file
    int max_locid = 0; // find what the maximum locus ID is in the current data to be saved into the info file
    auto it=this->geneMap.begin();
    while(it!=this->geneMap.end()){
        max_locid = (it->second.get_locid()>max_locid) ? it->second.get_locid() : max_locid;
        genefp<<it->second.get_locid()<<":"<<it->second.compute_elen()<<char(it->second.get_strand())<<it->second.get_start()<<" "<<it->second.get_end()<<std::endl;
        it++;
    }
    genefp.close();
    std::cerr<<"@LOG::done writing gene information"<<std::endl;

    std::cerr<<"@LOG::writing general information"<<std::endl;
    // write the information about the index now
    std::ofstream infofp(this->info_fname);
    infofp<<this->topTransID<<std::endl;
    infofp<<max_locid<<std::endl;
    infofp<<this->kmerlen<<std::endl;
    infofp.close();
    std::cerr<<"@LOG::done writing general information"<<std::endl;
}

void Indexer::transcript_map(){
    GffObj *p_gffObj;
    const char *p_contig_name;
    std::vector<int> *p_contig_vec;

    for (int i = 0; i < gtfReader_.gflst.Count(); ++i){
        p_gffObj = gtfReader_.gflst.Get(i);
        if (p_gffObj->isDiscarded() || p_gffObj->exons.Count()==0){
           continue;
        }

        p_contig_name = p_gffObj->getRefName();
        std::string contig_name(p_contig_name);

        // Check if the current contig exists in the map
        // If it doesn't, add it
        if (contigTransMap_.find(contig_name) == contigTransMap_.end()){
            p_contig_vec = new std::vector<int>;
            contigTransMap_[contig_name] = p_contig_vec;
        }
        else{
            p_contig_vec = contigTransMap_[contig_name];
        }
        p_contig_vec->push_back(i);
    }
}

void Indexer::print_mapping(){
    std::ofstream out_file("out.names");
    GffObj *p_gffObj;

    for (int i = 0; i < gtfReader_.gflst.Count(); ++i){
        p_gffObj = gtfReader_.gflst.Get(i);
        if (p_gffObj->isDiscarded() || p_gffObj->exons.Count()==0) continue;
        out_file << i << "\t" << p_gffObj->getID() << std::endl;
    }
    out_file.close();
}

void Indexer::print_mmap(){
    this->mmap.print();
}
void Indexer::print_mmap_multi(){
    this->mmap.print_multimapers();
}

void Indexer::save_header() {
    std::ofstream genome_headerfp(this->genome_headername);
    genome_headerfp<<"@HD\tVN:1.0\tSO:unsorted"<<std::endl;

    int prev_id = -1;
    for(auto &v : this->id_to_ref){
        if(v.first<=prev_id || (v.first - prev_id) != 1){
            for(int y=prev_id+1;y<v.first;y++){
                genome_headerfp<<"@SQ\tSN:DUMMY"<<y<<"\tLN:0"<<std::endl; // write dummies for contigs without annotation and hope it works
            }
            std::cerr<<"@ERROR::error in reference IDs: "<<prev_id<<"\t"<<v.first<<"\t"<<v.second.first<<"\t"<<id_to_ref[prev_id].first<<"\t"<<std::endl;
//            exit(1);
        }
        prev_id = v.first;
        genome_headerfp<<"@SQ\tSN:"<<v.second.first<<"\tLN:"<<v.second.second<<std::endl;
    }
    // now append contigs with no known transcripts to the end of the header
    for(auto &v : this->id_to_ref_no_trans){
        genome_headerfp<<"@SQ\tSN:"<<v.first<<"\tLN:"<<v.second<<std::endl;
    }
    // PG is added using the arguments from the execution of trans2genome during the conversion
    genome_headerfp.close();
}

void Indexer::save(bool multi, bool uniq){
    //first save the header
    save_header();
    if(multi){
        std::string multimap_fname(this->out_fname);
        multimap_fname.append(".multi");
        this->mmap.save_multimappers(multimap_fname);
    }
    if(uniq){
        std::string uniq_fname(this->out_fname);
        uniq_fname.append(".unq");
        this->mmap.save_unique(uniq_fname);
    }
}

enum Opt {GFF_FP   = 'a',
          REF_FA   = 'r',
          OUT_FA   = 'o',
          KMER_LEN = 'k',
          UNIQ     = 'u',
          MULTI    = 'm'};

int main(int argc, char *argv[])
{
    ArgParse args("trans2genome_indexer");
    args.add_string(Opt::GFF_FP,"gff","","path to the annotation of the genome is GFF/GTF format",true);
    args.add_string(Opt::REF_FA,"ref","","path to the reference genome in the FASTA format",true);
    args.add_string(Opt::OUT_FA,"output","","base name for the output files",true);
    args.add_int(Opt::KMER_LEN,"kmer",150,"kmer length to use for building the index",false);
    args.add_flag(Opt::UNIQ,"uniq","get a separate output with uniq kmers per each transcript",false);
    args.add_flag(Opt::MULTI,"multi","identify all multimappers present in the input transcriptome",false);

    if(strcmp(argv[1],"--help")==0){
        std::cerr<<args.get_help()<<std::endl;
        exit(1);
    }
    
    args.parse_args(argc,argv);
    
    int kmer_length=args.get_int(Opt::KMER_LEN);
    std::string gtf_fname(args.get_string(Opt::GFF_FP));
    std::string genome_fname(args.get_string(Opt::REF_FA));
    std::string out_fname(args.get_string(Opt::OUT_FA));
    bool multi=args.get_flag(Opt::MULTI);

    Indexer indexer(gtf_fname, genome_fname,out_fname, kmer_length,multi);
    indexer.make_transcriptome();
    indexer.save(args.get_flag(Opt::MULTI),args.get_flag(Opt::UNIQ));
    return 0;
}


/* * can think of three different ways to reassign multimappers
   * 1. on the gene/locus level
        * compute average coverage across the entire locus
        * coverage varies when multiple isoforms and may incorrectly reflect the coverage at the region of interest
   * 2. on the transcript level
        * compute average number of reads for each transcript individually
        * problem with overlapping transcripts - how can we handle them
        * 1. build an index to lookup how many isoforms cover a given range of positions
            * count the coverage as the number of reads divided by the number of overlaps for each read
            * alternatively, we could potentially precompute this index for all transcripts and use it to normalize the read counts for that transcript
                * in which case we do not need to perform expensive lookus during runtime and the index stays small
        * 2. preliminary transcriptome quantification
   * 3. on the window level
        * select a window around a multimapper and compute the average coverage
   * */


// Important!!!:: do not need a two-pass algorithm. We can achieve the same easier in a single pass
// Similar to the PID algorithm which for each read will change the target (coverage) and at the next multimapper would compensate given an error