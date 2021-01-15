var big_image;
$().ready(function () {

    responsive = $(window).width();

    if (responsive >= 768) {
        big_image = $('.parallax-background').find('img');
        console.log(big_image);
        $(window).on('scroll', function () {
            parallax();
        });
    }

    $('.selector').click(function () {
        SelectColor(this);
    });
});

function SelectColor(btn) {
    oldColor = $('.filter-gradient').attr('data-color');
    newColor = $(btn).attr('data-color');

    newColor = $(btn).attr('data-color');

    oldButton = $('#Demo').attr('data-button');
    newButton = $(btn).attr('data-button');

    $('.filter-gradient').removeClass(oldColor).addClass(newColor).attr('data-color', newColor);

    $('#Demo').removeClass("btn-" + oldButton).addClass("btn-" + newButton).attr('data-button', newButton);

    $('.carousel-indicators').removeClass("carousel-indicators-" + oldColor).addClass("carousel-indicators-" + newColor);

    $('.card').removeClass("card-" + oldColor).addClass("card-" + newColor);

    $('.selector').parent().removeClass('active');
    $(btn).parent().addClass('active');
}

var parallax = function () {
    var current_scroll = $(this).scrollTop();

    oVal = ($(window).scrollTop() / 3);
    big_image.css('top', oVal);
};

