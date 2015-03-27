#!/usr/bin/perl -w

######################################################
#
# PROGRAM       : ultrastikgui.pl
# USE           : NCurses interface to ultrastikcmd
# AUTHOR        : aleph
# DATE          : 02.04.09
#
# NOTES         : 
#
#####################################################


use strict;
use Getopt::Long;
use Curses::UI;


my ($PROG)        = ($0 =~ /\/*([^\/]+$)/);
my $debug;
my $controller = 1;

sub help {
        print "\n";
        print " $PROG: control ultrastik\n";
        print "\n";
        print " $PROG\n";
        print "\n";
        print "   -d 		debug mode.\n";
        print "   -h 		help mode.\n";
        print "   -c [1-4]	controller\n";
        
        print "\n";
        exit (1);
}

GetOptions( "help" 		=> \&help, "debug" => \$debug,
	"c|controller=i"	=> \$controller,
	
	) or help();

my $cui = Curses::UI->new(	-clear_on_exit => 1,
							-color_support => 1,
							-mouse_support => 0,
                            -debug => $debug, );
                            
my @menu = (
	{ -label => 'File', 
		-submenu => [
			{ -label => 'Open      ^O', -value => \&open_dialog  },
			{ -label => 'Save      ^S', -value => \&save_dialog  },
			{ -label => '------------', },
			{ -label => 'Exit      ^Q', -value => \&exit_dialog  },
			]
           },
        );                           
            
            my $menu = $cui->add(
                'menu','Menubar', 
                -menu => \@menu,
                -fg  => "red",
        );
        
my $win = $cui->add( 'win1', 'Window',
                             -border => 1,
                             -y    => 1,
                             -bfg => 'red',
                     );                           

$cui->set_binding(sub {$menu->focus()}, "\cX");
$cui->set_binding( \&exit_dialog , "\cQ");
$cui->set_binding( \&open_dialog , "\cO");
$cui->set_binding( \&save_dialog , "\cS");
    
my $label1 = $win->add(
	"label1", 'Label',
    -text     => "LED states, select to change",
    -bold     => 1,
    -x		   => 0,
    -y		   => 1,
    );
    
my $labelhex = $win->add(
	"labelhex", 'Label',
    -text     => "Current hex code:",
    -bold     => 0,
    -x		   => 0,
    -y		   => 6,
    );

my $labelhex2 = $win->add(
	"labelhex2", 'Label',
    -text     => "0x",
    -x		   => 18,
    -y		   => 6,
    );

my $texthex = $win->add(
	"texthex", 'TextEntry',
    -text     => "0000",
    -bold     => 1,
    -x		   => 20,
    -y		   => 6,
    -maxlength => 4, 
    -regexp 	=> '/^[0-9a-fA-F]*$/',
    -onChange	=> \&hex_changed,
    
    );

for(my $i=0;$i<16;$i++){    
    my $label = $win->add(
        "llabel$i", 'Label',
        -text     => sprintf("%2s",$i+1),
        -x		   => $i*4,
        -y		   => 3,
    );
}
      
my @checkboxs;

for(my $i=0;$i<16;$i++){    
    my $checkbox = $win->add(
        "mycheckbox$i", 'Checkbox',
        -label     => "",
        -checked   => 0,
        -x		   => $i*4,
        -y		   => 4,
        -onChange  => \&led_changed,
    );
	push @checkboxs, \$checkbox;
}

${$checkboxs[0]}->focus();

$cui->mainloop;

exit 0;

sub open_dialog() {
	my $file = $cui->loadfilebrowser();

	if($file){
		load_file($file);
	}
}

sub save_dialog() {
	my $file = $cui->savefilebrowser();

	if($file){
		saves_file($file);
	}
}



sub exit_dialog() {
	my $return = $cui->dialog(
		-message   => "Do you really want to quit?",
		-title     => "Are you sure???", 
		-buttons   => ['yes', 'no'],
	);

	exit(0) if $return;
}

sub led_changed() {
}

sub hex_changed() {
}

#----------------------------------------------------------------

sub load_file {
	my ($path) = @_;
	
	if(open (FH,$path)){
		my @data = <FH>;
		close FH;
		
		my $gotmagic = 0;
		my $gotmap = 0;
		my $MapBorderLocations = "30,58,86,114,142,170,198,226";
		my @Map = ("","","","","","","","","");
		
		
		foreach my $l (@data) {
			$l =~ s/^[\s\n\r]*//;
			$l =~ s/[\s\r\n]*$//;
			$l or next;
			
			if(!$gotmagic and $l !~ /^MapFileFormatVersion=[0-9.]+$/){
				$cui->error(-message => "Failed to find MapFileFormatVersion!");
				return;
			} else {
				$gotmagic = 1;
				next;
			}
			
			if(!$gotmap and $l !~ /^MapSize=([0-9]+)$/){
				$cui->error(-message => "Failed to find MapSize!");
				return;
			} else {
				$gotmap = $1;
				if($gotmap != 9){
					$cui->error(-message => "Mapsize not equal to 9!");
					return;
				}
				next;
			}
			
			if($l =~ /^MapBorderLocations=([,0-9]+)$/){
				$MapBorderLocations = $1;
				next;
			}
			
			if($l =~ /^MapRow([0-9]+)=(.+)$/){
				$Map[$1] = $2;
				next;
			}
		}
		
		update_map(\@Map);
		


	} else {
		$cui->error(-message => "Failed to open file $path!");
	}
}

sub save_file {
	my ($path) = @_;
}

sub update_map 
	my ($map) = @_;
	
	
}

















