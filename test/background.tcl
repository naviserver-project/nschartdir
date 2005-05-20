proc createChart { img filename } {

    # The data for the chart
    set data {85 156 179.5 211 123}
    set labels {"Mon" "Tue" "Wed" "Thu" "Fri"}

    # Create a XYChart object of size 270 x 270 pixels
    set chart [ns_chartdir create xy 270 270]

    # Set the plot area at (40 32) and of size 200 x 200 pixels
    # Set the background style based on the input parameter
    switch $img {
     0 {
        ns_chartdir setplotarea $chart 40 32 200 200
        # Has wallpaper image
        ns_chartdir setwallpaper $chart "tile.gif"
     }
     1 {
        ns_chartdir setplotarea $chart 40 32 200 200
        # Use a background image as the plot area background
        ns_chartdir setbgimage $chart "bg.png" Center -plotarea
     }
     2 {
        # Use white (0xffffff) and grey (0xe0e0e0) as two alternate
        # plotarea background colors
        ns_chartdir setplotarea $chart 40 32 200 200 0xffffff 0xe0e0e0
     }
     
     default {
        ns_chartdir setplotarea $chart 40 32 200 200
        # Use a dark background palette
        ns_chartdir setcolors $chart whiteOnBlackPalette
     }
    }

    # Set the labels on the x axis
    ns_chartdir xaxis $chart setlabels $labels

    # Add a color bar layer using the given data with auto color selection.
    # Use a 1 pixel 3D border for the bars.
    ns_chartdir layer $chart create bar $data
    ns_chartdir layer $chart setbordercolor 0 -1 1

    # output the chart in PNG format
    ns_chartdir save $chart $filename

    # destroy the chart to free up resources
    ns_chartdir destroy $chart
}

createChart 0 "background0.png"
createChart 1 "background1.png"
createChart 2 "background2.png"
createChart 3 "background3.png"
